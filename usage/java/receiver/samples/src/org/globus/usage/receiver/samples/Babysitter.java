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

/*
 * Jonathan DiCarlo
 * Simple program which connects to the running Receiver through a control socket
 * and outputs status on whether receiver is running, how many packets have been logged, etc.
 * Or reports failure if there is no response.*/

import java.net.Socket;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.io.IOException;
import java.net.SocketException;
import java.io.PrintWriter;
import java.io.BufferedReader;
import java.io.InputStreamReader;

public class Babysitter {

    private static final int controlPort = 4811;

    public static void main(String[] args) throws IOException {

        Socket controlSocket = null;
        PrintWriter out = null;
        BufferedReader in = null;
	String result;

        try {
            controlSocket = new Socket(InetAddress.getLocalHost(), controlPort);
            out = new PrintWriter(controlSocket.getOutputStream(), true);
            in = new BufferedReader(new InputStreamReader(
                                        controlSocket.getInputStream()));


	    out.println("info");
	    System.out.println("Got this from listener:");
	    do {
		result = in.readLine();
		if (result != null) {
		    System.out.println(result);
		}
	    } while (result != null);
        } catch (UnknownHostException e) {
            System.err.println("Can't resolve localhost, for some reason...");
        } catch (IOException e) {
            System.err.println("Couldn't open the socket; receiver may be down. ");
        }
	
	try {
	    out.close();
	    in.close();
	    controlSocket.close();
	}
	catch (Exception e) {}
    }

}

