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
package org.globus.usage.report.jwscore;

import java.sql.ResultSet;

import java.util.HashMap;
import java.util.Map;
import java.util.Iterator;
import java.util.Date;
import java.util.List;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.GregorianCalendar;
import java.text.SimpleDateFormat;
import java.io.PrintStream;
import java.sql.Timestamp;

import org.globus.usage.report.common.DatabaseRetriever;

public class ContainerEventReport {

    private Date startDate;
    private Date endDate;
    private int step;

    private Map startEvents = new HashMap();
    private Map stopEvents = new HashMap();
    private int matched;

    public ContainerEventReport(Date startDate, 
                                Date endDate,
                                int step) {
        checkStep(step);

        this.startDate = startDate;
        this.endDate = endDate;
        this.step = step;
    }

    private static void checkStep(int step) {
        switch (step) {
        case Calendar.HOUR_OF_DAY:
        case Calendar.DATE:
        case Calendar.MONTH:
            break;
        default:
            throw new IllegalArgumentException("Unsupported date step");
        }
    }

    private static String getDateFormat(int step) {
        switch (step) {
        case Calendar.HOUR_OF_DAY:
            return "yyyy-MM-dd HH:mm:ss";
        case Calendar.DATE:
            return "yyyy-MM-dd";
        case Calendar.MONTH:
            return "yyyy-MM";
        default:
            throw new IllegalArgumentException("Unsupported date step");
        }
    }

    static class Slot {

        private int startCount;
        private int stopCount;
        private long time;
        
        public Slot(long time) {
            this.time = time;
        }
        
        public void incrementStart() {
            this.startCount++;
        }

        public int getStartCount() {
            return this.startCount;
        }

        public void incrementStop() {
            this.stopCount++;
        }
        
        public int getStopCount() {
            return this.stopCount;
        }
        
        public long getTime() {
            return this.time;
        }
        
        public Date getDate() {
            return new Date(this.time);
        }
    }

    private List createSlots() {
        Calendar calendar = new GregorianCalendar();
        calendar.setTime(this.startDate);
        ArrayList slots = new ArrayList();

        Date date = null;
        while((date = calendar.getTime()).before(this.endDate)) {
            slots.add(new Slot(calendar.getTimeInMillis()));
            calendar.add(this.step, 1);
        }
        return slots;
    }

    private Slot getSlot(List slots, long mseconds) {
        Slot prevSlot = (Slot)slots.get(0);
        for (int i = 1; i < slots.size(); i++) {
            Slot slot = (Slot)slots.get(i);
            if (mseconds >= prevSlot.getTime() &&
                mseconds < slot.getTime()) {
                return prevSlot;
            }
            prevSlot = slot;
        }
        return prevSlot;
    }

    public void output(PrintStream out) {
        List slots = createSlots();
        Iterator iter;

        iter = this.startEvents.entrySet().iterator();
        while(iter.hasNext()) {
            Map.Entry entry = (Map.Entry)iter.next();
            Timestamp timestamp = (Timestamp)entry.getValue();
            Slot s = getSlot(slots, timestamp.getTime());
            s.incrementStart();
        }
        
        iter = this.stopEvents.entrySet().iterator();
        while(iter.hasNext()) {
            Map.Entry entry = (Map.Entry)iter.next();
            Timestamp timestamp = (Timestamp)entry.getValue();
            Slot s = getSlot(slots, timestamp.getTime());
            s.incrementStop();
        }
        
        // reporting starts here

        SimpleDateFormat dateFormat =
            new SimpleDateFormat(getDateFormat(this.step));

        out.println("  <total-matched>" + this.matched + "</total-matched>");
        out.println("  <total-start-unmatched>" + this.startEvents.size() + "</total-start-unmatched>");
        out.println("  <total-stop-unmatched>" + this.stopEvents.size() + "</total-stop-unmatched>");
        out.println("  <unmatched-events>");
        for (int i = 0; i< slots.size(); i++) {
            Slot slot = (Slot)slots.get(i);
            out.println("    <slot>");
            out.println("      <time>" + dateFormat.format(slot.getDate()) + "</time>");
            out.println("      <startCount>" + slot.getStartCount() + "</startCount>");
            out.println("      <stopCount>" + slot.getStopCount() + "</stopCount>");
            out.println("    </slot>");
        }
        out.println("  </unmatched-events>");
    }

    public void compute(int eventType,
                        Timestamp timestamp,
                        String containerID) {
        if (eventType == 1) { // start
            this.startEvents.put(containerID, timestamp);
        } else if (eventType == 2) {  // stop
            Timestamp startTime = 
                (Timestamp)this.startEvents.remove(containerID);
            if (startTime == null) {
                this.stopEvents.put(containerID, timestamp);
            } else {
                this.matched++;
            }
        }
    }
    
    public static void main(String[] args) throws Exception {
    
        String USAGE = "Usage: java ContainerEventReport [options] <date (yyyy-MM-dd)>";
        
        String HELP = 
            "Where [options] are:\n" +
            "  -help                           Displays this message\n" +
            "  -step <day|month>               Specifies step type (default: 'day')\n" +
            "  -n <steps>                      Specifies number of steps to do to\n" +
            "                                  determine end date (default: 1)\n" +
            "  -reportStep <hour|day|month>    Specifies step type for report\n" +
            "                                  (default: 'hour')\n" +
            "  -table TABLENAME                Use TABLENAME instead of the\n" +
            "                                  java_ws_core_packets table\n" +
            "\n";
        
        if (args.length == 0) {
            System.err.println(USAGE);
            System.exit(1);
        } else if (args.length == 1 && args[0].equalsIgnoreCase("-help")) {
            System.err.println(USAGE);
            System.err.println(HELP);
            System.exit(1);
        }

        String baseQueryStart = "select event_type,send_time,ip_address,container_id,version_code from ";
        String table = "java_ws_core_packets";
        String baseQueryEnd = " where ";
        int n = 1;
        String containerType = "all";
        String stepStr = "day";
        String reportStepStr = "hour";

        for (int i=0;i<args.length-1;i++) {
            if (args[i].equals("-n")) {
                n = Integer.parseInt(args[++i]);
            } else if (args[i].equals("-type")) {
                baseQueryEnd += " container_type = " + args[++i] + " and ";
            } else if (args[i].equals("-step")) {
                stepStr = args[++i];
            } else if (args[i].equals("-reportStep")) {
                reportStepStr = args[++i];
            } else if (args[i].equalsIgnoreCase("-help")) {
                System.err.println(USAGE);
                System.err.println(HELP);
                System.exit(1);
            } else if (args[i].equals("-table")) {
                table = args[++i];
            } else {
                System.err.println("Unknown argument: " + args[i]);
                System.exit(1);
            }
        }

        String inputDate = args[args.length-1];

        // parse step info
        int step = -1;
        if (stepStr.equalsIgnoreCase("day")) {
            step = Calendar.DATE;
        } else if (stepStr.equalsIgnoreCase("month")) {
            step = Calendar.MONTH;
        } else {
            System.err.println("Unsupported step: " + stepStr);
            System.exit(2);
        }

        // parse step info
        int reportStep = -1;
        if (reportStepStr.equalsIgnoreCase("day")) {
            reportStep = Calendar.DATE;
        } else if (reportStepStr.equalsIgnoreCase("month")) {
            reportStep = Calendar.MONTH;
        } else if (reportStepStr.equalsIgnoreCase("hour")) {
            reportStep = Calendar.HOUR_OF_DAY;
        } else {
            System.err.println("Unsupported step: " + reportStepStr);
            System.exit(2);
        }

        SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd");

        DatabaseRetriever db = null;

        try {
            db = new DatabaseRetriever();

            Date date = dateFormat.parse(inputDate);

            Calendar calendar = dateFormat.getCalendar();

            if (n < 0) {
                calendar.add(step, n);
                n = -n;
            }

            Date startDate = calendar.getTime();
            calendar.add(step, n);
            Date endDate = calendar.getTime();
            
            ContainerEventReport r = 
                new ContainerEventReport(startDate,
                                         endDate,
                                         reportStep);

            String startDateStr = dateFormat.format(startDate);
            String endDateStr = dateFormat.format(endDate);
            String timeFilter = "DATE(send_time) >= '" + startDateStr + 
                "' and DATE(send_time) < '" + endDateStr + "'";
                
            String query = baseQueryStart + table + baseQueryEnd + timeFilter + " order by send_time";

            System.out.println("<container-event-report container_type=\"" + 
                               containerType + "\">");
            System.out.println("  <start-date>" + startDateStr + "</start-date>");
            System.out.println("  <end-date>" + endDateStr + "</end-date>");

            ResultSet rs = db.retrieve(query);
            
            while (rs.next()) {
                String ip = rs.getString(3);
                String containerId = rs.getString(4);
                String packetVersion = rs.getString(5);
                
                r.compute(rs.getInt(1), rs.getTimestamp(2),
                          ip + "/" + containerId + "/" + packetVersion);
            }
            
            rs.close();

            r.output(System.out);

            System.out.println("</container-event-report>");

        } finally {
            if (db != null) {
                db.close();
            }
        }
    }
        
}

