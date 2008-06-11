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

import java.net.InetAddress;
import java.net.UnknownHostException;

import java.util.HashMap;
import java.util.Map;
import java.util.List;
import java.util.Iterator;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.io.PrintStream;

public class IPTable {

    HashMap uniqueIPs = new HashMap();

    HashMap domains = new HashMap();
    
    protected boolean groupCommonDomains;

    public IPTable() {
        this(true);
    }
    
    public IPTable(boolean groupCommonDomains) {
        this.groupCommonDomains = groupCommonDomains;
    }
    
    public void discoverDomains(Map ipLookupTable) {
        Iterator ipIter = getUniqueIPList().keySet().iterator();
        while (ipIter.hasNext()) {
            String ip = (String) ipIter.next();

            IPEntry ipEntry = (IPEntry) ipLookupTable.get(ip);
            if (ipEntry == null) {
                ipEntry = IPEntry.getIPEntry(ip, this.groupCommonDomains);
                ipLookupTable.put(ip, ipEntry);
            }
            addDomain(ipEntry.getDomain());
        }
    }
    
    public void addDomain(String domain) {
        DomainEntry c = (DomainEntry) domains.get(domain);
        if (c == null) {
            c = new DomainEntry(domain);
            domains.put(domain, c);
        }
        c.increment();
    }

    public void addAddress(String address) {
        uniqueIPs.put(address, "");
    }

    public int getUniqueIPCount() {
        return this.uniqueIPs.size();
    }

    public Map getUniqueIPList() {
        return this.uniqueIPs;
    }

    public Map getDomains() {
        return this.domains;
    }

    public List getSortedDomains() {
        List input = new ArrayList(this.domains.values());
        Collections.sort(input, new DomainEntry(null));
        return input;
    }

    public void output(PrintStream out, String tab) {
        out.println(tab + "<unique-ip>" + getUniqueIPCount() + "</unique-ip>");
        out.println(tab + "<domains>");
        Iterator iter = getSortedDomains().iterator();
        while (iter.hasNext()) {
            DomainEntry entry = (DomainEntry) iter.next();
            out.println(tab + "\t<domain-entry name=\"" + entry.getDomain()
                    + "\" count=\"" + entry.getCount() + "\"/>");
        }
        out.println(tab + "</domains>");
    }

    public static boolean isPrivateAddress(String address)
    throws UnknownHostException{
        int slashOff = address.indexOf("/");

        address = address.substring(slashOff+1);

        InetAddress ia = InetAddress.getByName(address);

        return isPrivateAddress(ia);
    }

    public static boolean isPrivateAddress(InetAddress address) {
        return address.isLoopbackAddress() ||
               address.isLinkLocalAddress() ||
               address.isLoopbackAddress() ||
               address.isSiteLocalAddress();
    }

    public static class DomainEntry implements Comparator {
        String domain;

        int value;

        public DomainEntry(String domain) {
            this.domain = domain;
        }

        public void increment() {
            this.value++;
        }

        public String getDomain() {
            return this.domain;
        }

        public int getCount() {
            return this.value;
        }

        public int compare(Object o1, Object o2) {
            int thisVal = ((DomainEntry) o2).value;
            int anotherVal = ((DomainEntry) o1).value;
            return (thisVal < anotherVal ? -1 : (thisVal == anotherVal ? 0 : 1));
        }

        public String toString() {
            return domain + " " + value;
        }
    }
}
