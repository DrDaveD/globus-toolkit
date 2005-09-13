/*
 * Portions of this file Copyright 1999-2005 University of Chicago
 * Portions of this file Copyright 1999-2005 The University of Southern California.
 *
 * This file or a portion of this file is licensed under the
 * terms of the Globus Toolkit Public License, found at
 * http://www.globus.org/toolkit/download/license.html.
 * If you redistribute this file, with or without
 * modifications, you must include this notice in the file.
 */
package org.globus.exec.monitoring.seg;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import org.globus.wsrf.ResourceKey;
import org.globus.exec.generated.StateEnumeration;
import org.globus.exec.monitoring.AlreadyRegisteredException;
import org.globus.exec.monitoring.JobStateMonitor;
import org.globus.exec.monitoring.NotRegisteredException;
import org.globus.exec.monitoring.SchedulerEvent;

/**
 * Scheduler Event Generator monitor thread.
 * 
 * The Seg object creates a Scheduler Event Generator process to 
 * monitor job state changes associated with a particular scheduler.
 *
 * The Seg object will repeatedly start the SEG process if it terminates
 * prematurely, until its shutdown() method is called.
 */
class SchedulerEventGenerator extends Thread {
    private static Log logger =
            LogFactory.getLog(SchedulerEventGenerator.class);

    /** Reference to the runtime used to start the SEG process */
    private static Runtime runtime = Runtime.getRuntime();

    /** Path to the SEG executable */
    private java.io.File path;
    /**
     * Username of the account to run the SEG as.
     *
     * <b>This is currently ignored.</b>
     */
    private String userName;
    /** Path to the SEG executable */
    private String schedulerName;
    /** SEG Process handle */
    private Process proc;
    /**
     * Flag indicating that the SEG process should no longer be
     * restarted and the thread should terminate.
     */
    private boolean shutdownCalled;
    /**
     * Timestamp of last event we've received from a SEG.
     */
    private java.util.Date timeStamp;

    /**
     * Monitor which created this SchedulerEventGenerator.
     * We call its addEvent() method when a new event is read from the
     * SEG process.
     */
    private JobStateMonitor monitor;

    /**
     * Used to keep track of the last restart time for the SEG process---if
     * it was too recent (less than our THROTTLE_RESTART_THRESHOLD) wait 
     * THROTTLE_RESTART_TIME before trying again.
     */
    private long lastRestart = 0;
    /**
     * When throttling process restarts, wait this many milliseconds before
     * next restart attempt.
     */
    private final long THROTTLE_RESTART_TIME = 2 * 60 * 1000;
    /**
     * When SEG terminates within this amount of time of being started, assume
     * something might be wrong and delay again.
     */
    private final long THROTTLE_RESTART_THRESHOLD = 2 * 1000;
    /**
     * SEG constructor.
     *
     * @param path
     *     Path to the Scheduler Event Generator executable.
     * @param userName
     *     Username to sudo(8) to start the SEG.
     * @param schedulerName
     *     Name of the scheduler SEG module to use (fork, lsf, etc).
     */
    public SchedulerEventGenerator(java.io.File path, String userName,
            String schedulerName, JobStateMonitor monitor) 
    {
        this.path = path;
        this.userName = userName;
        this.schedulerName = schedulerName;
        this.proc = null;
        this.shutdownCalled = false;
        this.timeStamp = null;
        this.monitor = monitor;

        lastRestart = 0;
    }

    /**
     * Start and monitor a SEG process.
     *
     * When the SEG terminates by itself for whatever reason, this thread
     * will restart it using the timestamp of the last item which was in
     * the event cache.
     */
    public void run() {
        try {
            while (startSegProcess(timeStamp)) {
                java.io.BufferedReader stdout;
                String input; 

                logger.debug("getting seg input");
                stdout = new java.io.BufferedReader(
                        new java.io.InputStreamReader(
                                proc.getInputStream()));
                if (logger.isDebugEnabled()) {
                    logger.debug("Seg input buffer is "
                    + (stdout.ready()?"read":"not ready"));
                }
                while ((input = stdout.readLine()) != null) {
                    logger.debug("seg input line: " + input);
                    java.util.StringTokenizer tok =
                            new java.util.StringTokenizer(input, ";");
                    int tokenCount = tok.countTokens();
                    String tokens[] = new String[tok.countTokens()];

                    for (int i = 0; i < tokens.length; i++) {
                        tokens[i] = tok.nextToken();
                    }

                    if (tokens[0].equals("001")) {
                        // Job state change message
                        if (tokens.length < 5) {
                            // Invalid message
                        }

                        StateEnumeration se;

                        switch (Integer.parseInt(tokens[3])) {
                            case 1:
                                se = StateEnumeration.Pending;
                                break;
                            case 2:
                                se = StateEnumeration.Active;
                                break;
                            case 4:
                                se = StateEnumeration.Failed;
                                break;
                            case 8:
                                se = StateEnumeration.Done;
                                break;
                            case 16:
                                se = StateEnumeration.Suspended;
                                break;
                            case 32:
                                se = StateEnumeration.Unsubmitted;
                                break;
                            default:
                                se = null;
                        }

                        SchedulerEvent e = new SchedulerEvent(
                            new java.util.Date(
                                Long.parseLong(tokens[1])*1000),
                            tokens[2],
                            se,
                            Integer.parseInt(tokens[4]));

                        timeStamp = e.getTimeStamp();

                        monitor.addEvent(e);
                    } else {
                        // Unknown message type
                    }
                }
            }
        } catch (java.io.IOException ioe) {
        }
    }

    /**
     * Start a scheduler event generator process.
     * 
     * This function is called to start a new scheduler event generator
     * process. This process will monitor the output of the scheduler
     * and send this object job state change notifications via the
     * processes's standard output stream.
     *
     * If the shutdown method of this object has been called, then the
     * process will not be started.
     *
     * @retval true New process started.
     * @reval false Did not create a new seg process.
     */
    private synchronized boolean startSegProcess(java.util.Date timeStamp)
            throws java.io.IOException
    {
        cleanProcess();

        proc = null;

        throttleRestart();

        if (!shutdownCalled) {
            logger.debug("Starting seg process");
            String [] cmd;

            // TODO: sudo integration here
            if (timeStamp != null) {
                cmd = new String[] {
                    path.toString(),
                    "-s", schedulerName,
                    "-t", Long.toString(
                            timeStamp.getTime() / 1000)};
            } else {
                cmd = new String[] {
                    path.toString(), "-s", schedulerName
                };
            }
            if (logger.isDebugEnabled()) {
                logger.debug("executing command: ");
                for (int i = 0; i  < cmd.length; i++) {
                    if (cmd[i] != null) {
                        logger.debug("->" + cmd[i]);
                    }
                }
            }
            proc = runtime.exec(cmd);
            return true;
        } else {
            return false;
        }
    }

    /**
     * Delay THROTTLE_RESTART_TIME before returning unless either
     * <ul>
     * <li>The SEG process wasn't restarted within
     *    THROTTLE_RESTART_THRESHOLD</li>
     * <li>The shutdown method has been called</li>
     * </ul>
     */
    private synchronized void throttleRestart() {
        logger.debug("throttleRestart called");

        long thisTime = new java.util.Date().getTime();
        long endOfWait = thisTime + THROTTLE_RESTART_TIME;

        while ((!shutdownCalled) &&
                    ((thisTime - lastRestart) < THROTTLE_RESTART_THRESHOLD)) {
            logger.debug("Throttling the restart as we just restarted the SEG");
            try {
                wait(endOfWait - thisTime);
            } catch (InterruptedException ie) {
            }
            thisTime = new java.util.Date().getTime();
        }
        lastRestart = thisTime;
    }

    private synchronized void cleanProcess() {
        if (proc != null) {
            try { proc.getInputStream().close(); } catch (Exception e) {}
            try { proc.getOutputStream().close(); } catch (Exception e) {}
            try { proc.getErrorStream().close(); } catch (Exception e) {}
        }
    }

    /**
     * Tell a SEG process to terminate.
     * 
     * This function will cause the thread associated with this
     * object to terminate once all input has been processed.
     */
    public synchronized void shutdown()
            throws java.io.IOException
    {
        if (shutdownCalled) {
            return;
        } else {
            logger.debug("cleaning process");
            cleanProcess();
            logger.debug("setting shutdownCalled");
            shutdownCalled = true;
            /* Wake up throttler if we were waiting in it */
            logger.debug("notifying");
            notify();
            logger.debug("done");
        }
    }
    
    public void start(java.util.Date timeStamp) {
        logger.debug("Starting seg thread");
        this.timeStamp = timeStamp;

        start();
    }
}
