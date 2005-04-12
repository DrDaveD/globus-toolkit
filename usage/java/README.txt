org.globus.usage.*;
A simple extensible system for remotely logging gt4 usage statistics with
UDP packets.
by Jonathan DiCarlo
Jan 28, 2005
GPT package creation by Peter Lane
February 3, 2005

Bug reports, questions, suggestions, etc. to jdicarlo@mcs.anl.gov.

*******************************************
Compiling and running:
*******************************************

This should be compatible with Java 1.3 and up.  If you find any
incompatibility with Java 1.3, it is a bug -- please tell me about it!

ExampleGFTPSender.java and ReceiverExample.java show how a program would
use the other classes.

To run the receiver example, you will have to first set up a database (see SQL
SCHEMA), and edit the file udpUsage.properties with the right driver class,
url, and table names for your database.

On the sender side, edit udpUsage.properties to set the hostname and port
number for the machine running the receiver.  If you want to send to multiple
receivers, put a comma-separated list of hostnames (or IP addresses) on one
line.  For example:

receiver-ip = localhost, 192.168.0.101, foovax.ubar.edu

Once you're done editing the properties, you can compile and start the
receiver simply by executing:

ant run-reciever 

(this will block, so open another terminal or run it in background).  You can
test the receiver by running the example sender:

ant run-sender

The example sender just sends a few GFTPMonitorPackets and then exits.

*******************************************
Packages and Dependencies
*******************************************

org.globus.usage.packets contains the UsageMonitorPacket class, its
subclasses, and the utility classes that it depends on.  Both senders and
receivers need this package.

org.globus.usage.receiver contains the Receiver class, and
org.globus.usage.receiver.handlers contains all of the Handler
classes that plug into the Receiver.  These depend on classes in
org.globus.usage.packets.

There are example classes ExampleGFTPSender and ExampleReceiver which are not
part of either package.  They demonstrate how to use the packets and receiver
packages. They are in org.globus.usage.packets.samples and
org.globus.usage.receiver.samples respectively.

Therefore, programs that want to send usage-monitor packets need to
include only org.globus.usage.packets.*;  programs that want to receive
packets need to include org.globus.usage.packets.*, org.globus.usage.receiver.*,
and org.globus.usage.receiver.handlers.*.

The receiver additionally needs a database driver, such as the
msyql-connector-java.jar included in lib/.

*******************************************
How to Add your own Packet Formats:
*******************************************

1. Write a class that extends UsageMonitorPacket

   The basic UsageMonitorPacket has fields for component code and version
   code.  Your subclass can set the appropriate values for these in its
   constructor.  IPTimeMonitorPacket, a subclass of UsageMonitorPacket, also
   includes the commonly-needed fields for the IP of the source and the time
   at which the packet was sent, so if you want those you should subclass
   IPTimeMonitorPacket.  Add whatever other fields you need, with getter and
   setter methods; see GFTPMonitorPacket for an example.

   It's important to override these functions:
         void packCustomFields(CustomByteBuffer buf);	
         void unpackCustomFields(CustomByteBuffer buf);

   packCustomFields should write all the custom fields of your packet
   into the CustomByteBuffer; unpackCustomFields should read them out
   again in the same order.  This can be accomplished with the
   CustomByteBuffer.get() and CustomByteBuffer.put() methods and their
   variants; see CustomByteBuffer.java.

   Note that packCustomFields() and unpackCustomFields() are always
   called for you by the functions that send and receive packets; you
   do not have to invoke them yourself.  If you are subclassing
   another packet, you must preserve the superclass's data by starting
   your packCustomFields with a call to super.packCustomFields(buf),
   and starting your unpackCustomFields with a call to
   super.unpackCustomFields(buf).


2. Write a class that implements PacketHandler

   There are three methods you must implement:
    public boolean doCodesMatch(short componentCode, short versionCode);

    this should return true if componentCode and versionCode match the
    code numbers that you want to handle.

    public UsageMonitorPacket instantiatePacket(ByteBuffer rawBytes);

    this should create and return an instance of your custom
    UsageMonitorPacket subclass.  (unpackCustomFields will be called for you,
    so you don't need to use rawBytes unless there is something special you
    want to do with it.)

    public void handlePacket(UsageMonitorPacket pack);

    This does whatever you want to do with the incoming packet.  At this point
    it is safe to cast the UsageMonitorPacket to your custom subclass.


   Instead of implementing PacketHandler, you can also write a class that
   extends DefaultPacketHandler; DefaultPacketHandler writes packets to a
   database, so if you want to do something similar, extending this class is
   the easiest way to go.  Override     

   protected String makeSQLInsert(UsageMonitorPacket pack);

   to return the text of an SQL instruction as appropriate for your packet.

3. Register the handler:
   When you create a Receiver object in your monitor program, you can pass an
   instance of your PacketHandler class to receiver.registerHandler().  The
   methods of your PacketHandler will be called when an appropriate packet
   comes in.

4. Write a sender which creates and sends your packets:

   the class ExampleGFTPSender is an example of how to do this.  The basic
   idea is to open a socket using something like
   socket = new DatagramSocket();
   then create an instance of your packet subclass, use its setter methods to
   put whatever data in it you want, and then send it by calling
   packet.sendPacket(socket, address, port);

*******************************************
Known Issues / To-do list:
*******************************************

1.  InetAddress.getLocalHost() will return an internal IP address if
    called on a computer behind a NAT box.  This internal IP address
    is fairly meaningless to external logging software elsewhere.  We
    should think about how we want to handle this.  The change would
    be in the Sender class.

2.  SQL assumptions -- this has been tested with MySQL but nothing else;
    probably needs some modification to be portable to other databases.  Also,
    the program expects that an SQL database is already up and running and has
    a table of the appropriate schema.  (See SQL SCHEMA).
    I am not an SQL expert.  The program works, but its implementation is
    probably naiive.  For instance, I had to hard-code parts of the SQL
    strings, which I know is not portable.


*******************************************
SQL SCHEMA:
*******************************************

For the default handler, which just stores
    the binary contents of the packet as a BLOB, the schema is like this:

	id BIGINT NOT NULL AUTO_INCREMENT,
	componentcode SMALLINT NOT NULL,
	versioncode SMALLINT NOT NULL,
	contents BLOB,
	PRIMARY KEY (id)

For GridFTP packets, the schema is like this:

    id BIGINT NOT NULL AUTO_INCREMENT,
    component_code SMALLINT NOT NULL,
    version_code SMALLINT NOT NULL,
    send_time DATETIME,
    ip_version TINYINT,
    ip_address TINYTEXT,
    gftp_version VARCHAR(20),
    stor_or_retr TINYINT,
    start_time DATETIME,
    end_time DATETIME,
    num_bytes BIGINT,
    num_stripes BIGINT,
    num_streams BIGINT,
    buffer_size BIGINT,
    block_size BIGINT,
    ftp_return_code BIGINT
    PRIMARY KEY (id)

