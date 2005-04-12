package org.globus.usage;

import junit.framework.TestCase;
import junit.framework.Test;
import junit.framework.Assert;
import junit.framework.TestSuite;

import org.globus.usage.packets.CustomByteBuffer;

public class ByteBufferTester extends TestCase {

    CustomByteBuffer buf;

    public ByteBufferTester(String name) {
	super(name);
    }

    protected void setUp() {
	buf = new CustomByteBuffer(100);
    }

    protected void tearDown() {

    }

    public void testGetRemaining() {
	
	buf.putInt(1);
	buf.putInt(2);
	buf.put(new String("Foo").getBytes());
	buf.put(new String("Bar").getBytes());
	buf.put(new String("Baz").getBytes());

	buf.rewind();
	buf.shrink();

	Assert.assertEquals("should pull out a 1", buf.getInt(), 1);
	Assert.assertEquals("should pull out a 2", buf.getInt(), 2);
	Assert.assertEquals("Remaining part should match all the strings...", new String(buf.getRemainingBytes()), "FooBarBaz");
    }

    public void testShortsInOut() {
	short a = 42;
	short b = 69;
	short c = 117;

	buf.putShort(a);
	buf.putShort(b);
	buf.putShort(c);
	buf.rewind();
	Assert.assertEquals(buf.getShort(), a);
	Assert.assertEquals(buf.getShort(), b);
	Assert.assertEquals(buf.getShort(), c);
    }


    public void testBytesInOut() {
	byte a = 42;
	byte b = 69;
	byte c = 117;

	buf.put(a);
	buf.put(b);
	buf.put(c);
	buf.rewind();
	Assert.assertEquals(buf.get(), a);
	Assert.assertEquals(buf.get(), b);
	Assert.assertEquals(buf.get(), c);
    }


    public void testLongsInOut() {
	long a = 259450;
	long b = 872642;
	long c = 999183;

	buf.putLong(a);
	buf.putLong(b);
	buf.putLong(c);
	buf.rewind();
	Assert.assertEquals(buf.getLong(), a);
	Assert.assertEquals(buf.getLong(), b);
	Assert.assertEquals(buf.getLong(), c);
    }

    public void testToBytesAndBack() {
	byte[] bytes;
	CustomByteBuffer after;
	long a = 80421000;
	short b = 42;
	int c = 32000;
	
	buf.putLong(a);
	buf.putShort(b);
	buf.putInt(c);

	bytes = buf.array();
	after = CustomByteBuffer.wrap(bytes);

	Assert.assertEquals(after.getLong(), a);
	Assert.assertEquals(after.getShort(), b);
	Assert.assertEquals(after.getInt(), c);
    }

    public void testFitToBytes() {
	byte[] bigBuf = new byte[1400];
	bigBuf[0] = 10;
	bigBuf[1] = 20;
	bigBuf[2] = 30;
	bigBuf[3] = 40;

	CustomByteBuffer smallBuf = CustomByteBuffer.fitToData(bigBuf, 3);

	Assert.assertEquals(smallBuf.limit(), 3);
	Assert.assertEquals(smallBuf.remaining(), 3);
	Assert.assertEquals(smallBuf.get(), (byte)10);
	Assert.assertEquals(smallBuf.get(), (byte)20);
	Assert.assertEquals(smallBuf.get(), (byte)30);
	Assert.assertEquals(smallBuf.remaining(), 0);
	
	
    }
}
