package mil.navy.nrl.protolib;

import java.io.IOException;
import java.net.SocketException;

/**
 * This class maps to a native instance of a Protolib "ProtoPipe".
 * (Note this first attempt supports only "ProtoPipe(MESSAGE)"
 * @author Brian Adamson
 */

// TODO Clean up all of the Exception throws; we can probably get rid of some

public class ProtoPipe 
{
	//static {System.load("C:\\Documents and Settings\\Protean\\Desktop\\Leon\\workspace\\protolib-jni\\libProtokitJni.jnilib");}
	static {System.loadLibrary("ProtolibJni");}

	@SuppressWarnings("unused")
	private long handle; // Keeps pointer to the native ProtoPipe
	
	public ProtoPipe() throws SocketException {createProtoPipe();}
	
	private native void createProtoPipe() throws SocketException;
	
	public native boolean listen(String pipeName) throws IOException;
	
	public native int read(byte[] b, int off, int len) throws IOException, 
	 												          NullPointerException, 
	 												          IndexOutOfBoundsException;
	
	public native boolean connect(String pipeName) throws IOException;
	
	public native void write(byte[] b, int off, int len) throws IOException, 
	 													        NullPointerException, 
	 													        IndexOutOfBoundsException;
	
	public native void close();
	
	private native void doFinalize();
	
    protected void finalize() throws Throwable {doFinalize(); super.finalize();}
	
}
