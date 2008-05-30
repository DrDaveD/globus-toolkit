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
package org.globus.usage.report.common;

import java.util.Vector;
import java.io.IOException;
import java.io.InputStream;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.PrintStream;

public class Slotter {

    private Vector slots;

    private int num_slots;
    private long[] values;
    private long[] threshold;

    public Slotter(String name) throws IOException {

        this.slots = new Vector();

        InputStream is = getClass().getClassLoader().getResourceAsStream(
                    "etc/globus_usage_reports/slots.data");
        if (is == null) {
            throw new IOException("Unable to load resource");
        }

        BufferedReader in = null;
        
        try {
            in = new BufferedReader(new InputStreamReader(is));
            String read;
            while ((read = in.readLine()) != null) {
                if ((read.trim()).equalsIgnoreCase(name)) {
                    read = in.readLine();
                    while (read.indexOf(",") != -1) {
                        String size = (read.substring(0, read.indexOf(",")))
                                .trim();
                        read = read.substring(read.indexOf(",") + 1);
                        this.slots.add(size);
                    }
                    String size = read.trim();
                    this.slots.add(size);
                }
            }
        } finally {
            if (in != null) {
                try { in.close(); } catch (IOException e) {}
            }
            in.close();
        }

        num_slots = (int) slots.size();
        values = new long[num_slots];
        for (int i = 0; i < values.length; i++) {
            values[i] = 0;
        }
        threshold = new long[num_slots];
        String prevSlot = (String) this.slots.get(0);
        for (int i = 0; i < this.slots.size(); i++) {
            String slot = (String) this.slots.get(i);
            threshold[i] = slotValue(slot);
        }
    }

    public static long slotValue(String value) {
        String number;
        String label;
        if (value.indexOf(" ") != -1) {
            number = value.substring(0, value.indexOf(" ")).trim();
            label = value.substring(value.indexOf(" "), value.length()).trim();
        } else {
            number = value.trim();
            label = "";
        }
        try {
            long x = Long.parseLong(number);
            if (label.startsWith("k") || label.startsWith("K")) {
                return x * 1024;
            } else if (label.startsWith("m") || label.startsWith("M")) {
                return x * 1024 * 1024;
            } else if (label.startsWith("g") || label.startsWith("G")) {
                return x * 1024 * 1024 * 1024;
            } else {
                return x;
            }
        } catch (Exception e) {
            System.err.println(e.getMessage());
        }
        return -1;
    }

    public void addValue(double value) {
        addValue(value, 1);
    }

    public void addValue(double value, long valueToAdd) {
        values[whichSlot(value)] += valueToAdd;
    }

    public void output(PrintStream io) {
        for (int i = 0; i < this.slots.size(); i++) {
            io.println("\t<item>");
            if (i != slots.size() - 1) {
                io.println("\t\t<name>" + (String) this.slots.get(i) + "-"
                        + (String) this.slots.get(i + 1) + "</name>");
            } else {
                io.println("\t\t<name>" + (String) this.slots.get(i) + "+"
                        + "</name>");
            }
            io.println("\t\t<single-value>" + values[i] + "</single-value>");
            io.println("\t</item>");
        }
    }

    public int whichSlot(double value) {
        for (int i = 1; i < num_slots; i++) {
            long slot = this.threshold[i];

            if (value < slot) {
                return i-1;
            }
        }
        return num_slots-1;
    }

    public String whichSlotString(double value) {
        return (String) slots.get(whichSlot(value));
    }

    public long getSlotThreshold(int which) {
        return threshold[which];
    }

    public String getSlotName(int which) {
        return (String) slots.get(which);
    }

    public int getNumSlots() {
        return num_slots;
    }
}
