/*
 * $Id: Awk.java,v 1.21 2007-01-24 14:21:29 bacon Exp $
 */

package ase.awk;

import java.security.AccessController;
import java.security.PrivilegedAction;
import java.net.URL;

public abstract class Awk
{
	// mode for open_source & close_source 
	public static final int SOURCE_READ = 1;
	public static final int SOURCE_WRITE = 2;

	// depth id
	public static final int DEPTH_BLOCK_PARSE = (1 << 0);
	public static final int DEPTH_BLOCK_RUN   = (1 << 1);
	public static final int DEPTH_EXPR_PARSE  = (1 << 2);
	public static final int DEPTH_EXPR_RUN    = (1 << 3);
	public static final int DEPTH_REX_BUILD   = (1 << 4);
	public static final int DEPTH_REX_MATCH   = (1 << 5);

	static
	{
		/*
		System.getProperty("os.name"));   os.arch / os.version;
		*/
		//System.load ("c://projects//ase/awk/aseawk.dll");
		URL url = ase.awk.Awk.class.getResource("aseawk.dll");
		if (url == null) url = ase.awk.Awk.class.getResource("libaseawk_jni.so");
		if (url != null) System.load (url.getFile());

		/*
		AccessController.doPrivileged (new PrivilegedAction ()
		{
			public Object run ()
			{
				URL url = ase.awk.Awk.class.getResource("aseawk.dll");
				if (url == null) url = ase.awk.Awk.class.getResource("libaseawk_jni.so");
				
				if (url != null) System.load (url.getFile());
				//System.load ("c://projects//ase/awk/aseawk.dll");
				//System.loadLibrary ("aseawk");
				return null;
			}
		});
		*/
	}

	private long handle;

	public Awk () throws Exception
	{
		open ();
	}

	/* == just in case == */
	protected void finalize () throws Throwable
	{
		super.finalize ();
		if (handle != 0) close ();
	}

	/* == native methods == */
	private native void open () throws Exception;
	public  native void close ();
	public  native void parse () throws Exception;
	public  native void run () throws Exception;

	private native int getmaxdepth (int id);
	private native void setmaxdepth (int id, int depth);

	private native void addbfn (
		String name, int min_args, int max_args) throws Exception;
	private native void delbfn (String name) throws Exception;

	private native void setfilename (
		long runid, String name) throws Exception;
	private native void setofilename (
		long runid, String name) throws Exception;

	private native Object strtonum (
		long runid, String str) throws Exception;
	private native String valtostr (long runid, Object obj);

	/* == builtin functions == */
	public void addBuiltinFunction (
		String name, int min_args, int max_args) throws Exception
	{
		addbfn (name, min_args, max_args);
	}

	public void deleteBuiltinFunction (String name) throws Exception
	{
		delbfn (name);
	}

	protected long builtinFunctionArgumentToLong (
		long runid, Object obj) throws Exception
	{
		long n;

		if (obj == null) n = 0;
		else
		{
			if (obj instanceof String)
				obj = strtonum (runid, (String)obj);

			if (obj instanceof Long)
			{
				n = ((Long)obj).longValue ();
			}
			else if (obj instanceof Double)
			{
				n = ((Double)obj).longValue ();
			}
			else if (obj instanceof Integer)
			{
				n = ((Integer)obj).longValue ();
			}
			else if (obj instanceof Short)
			{
				n = ((Short)obj).longValue ();
			}
			else if (obj instanceof Float)
			{
				n = ((Float)obj).longValue ();
			}
			else n = 0;
		}

		return n;
	}

	protected double builtinFunctionArgumentToDouble (
		long runid, Object obj) throws Exception
	{
		double n;

		if (obj == null) n = 0.0;
		else
		{
			if (obj instanceof String)
				obj = strtonum (runid, (String)obj);

			if (obj instanceof Long)
			{
				n = ((Long)obj).doubleValue ();
			}
			else if (obj instanceof Double)
			{
				n = ((Double)obj).doubleValue ();
			}
			else if (obj instanceof Integer)
			{
				n = ((Integer)obj).doubleValue ();
			}
			else if (obj instanceof Short)
			{
				n = ((Short)obj).doubleValue ();
			}
			else if (obj instanceof Float)
			{
				n = ((Float)obj).doubleValue ();
			}
			else n = 0.0;
		}

		return n;
	}

	protected String builtinFunctionArgumentToString (
		long runid, Object obj) throws Exception
	{
		String str;

		if (obj == null) str = "";
		else if (obj instanceof String) str = (String)obj;
		else str = valtostr (runid, obj);

		return str;
	}

	/* == console name setters == */
	protected void setConsoleInputName (
		Extio extio, String name) throws Exception
	{
		/* TODO: setfilename is not safe. for example, it can 
		 * crash the program if runid is invalid. so this wrapper
		 * needs to do some sanity check. */
		setfilename (extio.getRunId(), name);
	}

	protected void setConsoleOutputName (
		Extio extio, String name) throws Exception
	{
		setofilename (extio.getRunId(), name);
	}

	/* == depth limiting == */
	public int getMaxDepth (int id)
	{
		return getmaxdepth (id);
	}

	public void setMaxDepth (int ids, int depth)
	{
		setmaxdepth (ids, depth);
	}

	/* == source code management == */
	protected abstract int openSource (int mode);
	protected abstract int closeSource (int mode);
	protected abstract int readSource (char[] buf, int len);
	protected abstract int writeSource (char[] buf, int len);

	/* == external io interface == */
	protected int openExtio (Extio extio)
	{
		int type = extio.getType ();
		if (type == Extio.TYPE_CONSOLE) return openConsole (extio);
		if (type == Extio.TYPE_FILE) return openFile (extio);
		if (type == Extio.TYPE_PIPE) return openPipe (extio);
		//if (type == Extio.TYPE_COPROC) return openCoproc (extio);
		return -1;
	}

	protected int closeExtio (Extio extio)
	{
		int type = extio.getType ();
		if (type == Extio.TYPE_CONSOLE) return closeConsole (extio);
		if (type == Extio.TYPE_FILE) return closeFile (extio);
		if (type == Extio.TYPE_PIPE) return closePipe (extio);
		//if (type == Extio.TYPE_COPROC) return closeCoproc (extio);
		return -1;
	}

	protected int readExtio (Extio extio, char[] buf, int len)
	{
		// this check is needed because 0 is used to indicate
		// the end of the stream. java streams can return 0 
		// if the data given is 0 bytes and it didn't reach 
		// the end of the stream. 
		if (len <= 0) return -1;

		int type = extio.getType ();
		if (type == Extio.TYPE_CONSOLE) 
			return readConsole (extio, buf, len);
		if (type == Extio.TYPE_FILE) 
			return readFile (extio, buf, len);
		if (type == Extio.TYPE_PIPE)
		 	return readPipe (extio, buf, len);
		//if (type == Extio.TYPE_COPROC)
		//	return readCoproc (extio, buf, len);
		return -1;
	}

	protected int writeExtio (Extio extio, char[] buf, int len)
	{
		if (len <= 0) return -1;

		int type = extio.getType ();
		if (type == Extio.TYPE_CONSOLE) 
			return writeConsole (extio, buf, len);
		if (type == Extio.TYPE_FILE) 
			return writeFile (extio, buf, len);
		if (type == Extio.TYPE_PIPE) 
			return writePipe (extio, buf, len);
		//if (type == Extio.TYPE_COPROC)
		//	return writeCoproc (extio, buf, len);
		return -1;
	}

	protected int flushExtio (Extio extio)
	{
		int type = extio.getType ();
		if (type == Extio.TYPE_CONSOLE) return flushConsole (extio);
		if (type == Extio.TYPE_FILE) return flushFile (extio);
		if (type == Extio.TYPE_PIPE) return flushPipe (extio);
		//if (type == Extio.TYPE_COPROC) return flushCoproc (extio);
		return -1;
	}

	protected int nextExtio (Extio extio)
	{
		int type = extio.getType ();
		if (type == Extio.TYPE_CONSOLE) return nextConsole (extio);
		return -1;
	}

	protected abstract int openConsole (Extio extio);
	protected abstract int closeConsole (Extio extio);
	protected abstract int readConsole (Extio extio, char[] buf, int len);
	protected abstract int writeConsole (Extio extio, char[] buf, int len);
	protected abstract int flushConsole (Extio extio);
	protected abstract int nextConsole (Extio extio);

	protected abstract int openFile (Extio extio);
	protected abstract int closeFile (Extio extio);
	protected abstract int readFile (Extio extio, char[] buf, int len);
	protected abstract int writeFile (Extio extio, char[] buf, int len); 
	protected abstract int flushFile (Extio extio);

	protected abstract int openPipe (Extio extio);
	protected abstract int closePipe (Extio extio);
	protected abstract int readPipe (Extio extio, char[] buf, int len);
	protected abstract int writePipe (Extio extio, char[] buf, int len); 
	protected abstract int flushPipe (Extio extio);
}
