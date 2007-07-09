/*
 * Copyright 1999-2006 University of Chicago
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.globus.usage.receiver;

import java.io.IOException;
import java.util.LinkedList;
import java.util.ListIterator;
import java.util.Properties;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import org.globus.usage.packets.CustomByteBuffer;
import org.globus.usage.packets.UsageMonitorPacket;
import org.globus.usage.receiver.handlers.DefaultPacketHandler;
import org.globus.usage.receiver.handlers.PacketHandler;

import java.sql.DriverManager;

import org.apache.commons.pool.ObjectPool;
import org.apache.commons.pool.impl.GenericObjectPool;
import org.apache.commons.dbcp.ConnectionFactory;
import org.apache.commons.dbcp.PoolingDriver;
import org.apache.commons.dbcp.PoolableConnectionFactory;
import org.apache.commons.dbcp.DriverManagerConnectionFactory;


public class HandlerThread extends Thread {

    public static final String dbPoolName = "jdbc:apache:commons:dbcp:usagestats";
    private static final String POOL_NAME = "usagestats";

    private static Log log = LogFactory.getLog(HandlerThread.class);

    private LinkedList handlerList; /*a reference to the one in Receiver*/
    private RingBuffer theRing; /*a reference to the one in Receiver*/
    private boolean stillGood = true;
    private DefaultPacketHandler theDefaultHandler;

    private int packetsLogged;
    private int errorCount;
    private int unknownPackets;

    public HandlerThread(LinkedList list, RingBuffer ring, Properties props) {
        super("UDPHandlerThread");

        this.handlerList = list;
        this.theRing = ring;

        String driverClass = props.getProperty("database-driver");
        String dburl = props.getProperty("database-url");
        String table = props.getProperty("default-table");
         
	try {
	    Class.forName(driverClass);
	    setUpDatabaseConnectionPool(dburl, props);
	    theDefaultHandler = new DefaultPacketHandler(dburl, table);
	} catch (Exception e) {
	    log.error("Can't start handler thread: " + e.getMessage());
	    stillGood = false;
	}
    }

    private void setUpDatabaseConnectionPool(String dburl, Properties props) 
        throws Exception {
	/*Set up database connection pool:  all handlers which need a 
	  database connection (which, so far, is all handlers) can take
	  connections from this pool.*/
        
        String dbuser = props.getProperty("database-user");
        String dbpwd = props.getProperty("database-pwd");
        String dbValidationQuery = props.getProperty("database-validation-query");

	GenericObjectPool connectionPool = new GenericObjectPool(null);
	ConnectionFactory connectionFactory = new DriverManagerConnectionFactory(dburl, dbuser, dbpwd);
	PoolableConnectionFactory poolableConnectionFactory = 
            new PoolableConnectionFactory(connectionFactory, connectionPool, null, 
                                          dbValidationQuery, false, true);
	PoolingDriver driver = new PoolingDriver();
	driver.registerPool(POOL_NAME, connectionPool);
    }

    /*The handler thread maintains counts of the number of packets 
      successfully written to database and the number that could not
      be parsed:*/
    public int getPacketsLogged() {
	return this.packetsLogged;
    }
    
    public int getUnparseablePackets() {
	return this.errorCount;
    }

    public int getUnknownPackets() {
        return this.unknownPackets;
    }
    
    public void resetCounts() {
	this.packetsLogged = 0;
	this.errorCount = 0;
        this.unknownPackets = 0;
    }

    /*This thread waits on the RingBuffer; when packets come in, it starts
      reading them out and letting the handlers have them.*/
    public void run() {
        short componentCode, versionCode;
        CustomByteBuffer bufFromRing = null;

        while(stillGood) {
            try {
                /*If ring is empty, this call will result in a thread wait
                  and will not return until there's something to read.*/
                bufFromRing = theRing.getNext();
                if (bufFromRing == null) {
                    break;
                }
                componentCode = bufFromRing.getShort();
                versionCode = bufFromRing.getShort();
                bufFromRing.rewind();
                tryHandlers(bufFromRing, componentCode, versionCode);
	    
                this.packetsLogged ++;
            } catch (Exception e) {
                this.errorCount ++;

                //this thread has to be able to catch any exception and keep
                //going... otherwise a bad packet could shut down the thread!
                log.error("Error during handler processing", e);
                if (bufFromRing != null) {
                    log.error(new String(bufFromRing.array()));
                }
                /*TODO: if this is an I/O exception, 
                  i.e. can't talk to database,
                  maybe restart the connection right here.*/
            }
	}

        closeDatabaseConnectionPool();
    }

    /*Use component code and version code in packet to decide
      which handler to use:*/
    private void tryHandlers(CustomByteBuffer bufFromRing,
                             short componentCode,
                             short versionCode) {
        UsageMonitorPacket packet;
        boolean hasBeenHandled;
        PacketHandler handler;
        ListIterator it;
        
        /*This next bit is synchronized to make sure a handler can't
              be registered while we're walking the list...*/
        synchronized(handlerList) {
            hasBeenHandled = false;                
            for (it = handlerList.listIterator(); it.hasNext(); ) {
                handler = (PacketHandler)it.next();
                if (handler.doCodesMatch(componentCode, versionCode)) {
                    packet = handler.instantiatePacket(bufFromRing);
                    packet.parseByteArray(bufFromRing.array());
                    handler.handlePacket(packet);
                    bufFromRing.rewind();
                    hasBeenHandled = true;
                }
            }
            if (!hasBeenHandled) {
                packet = theDefaultHandler.instantiatePacket(bufFromRing);
                packet.parseByteArray(bufFromRing.array());
                theDefaultHandler.handlePacket(packet);
                this.unknownPackets++;

                if (log.isDebugEnabled()) {
                    log.debug("Unknown packet: " +
                       DefaultPacketHandler.getPacketContentsBinary(packet));
                }
            }
        }
        /*If multiple handlers return true for doCodesMatch, each
          handler will be triggered, each with its own separate copy of
          the packet.  theDefaultHandler will be called only if no other
          handlers trigger.*/        
    }

    protected void closeDatabaseConnectionPool() {
        log.info("Closing database connection pool");
	try {
	    PoolingDriver driver = 
                (PoolingDriver)DriverManager.getDriver("jdbc:apache:commons:dbcp:");
	    //driver.closePool(POOL_NAME);
	} catch(Exception e) {
	    log.warn(e.getMessage());
	}
    }

    public void shutDown() {
        stillGood = false; //lets the loop in run() finish
    }
}
