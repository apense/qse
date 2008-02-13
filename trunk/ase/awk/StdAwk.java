/*
 * $Id: StdAwk.java,v 1.21 2007/11/10 15:30:07 bacon Exp $
 *
 * {License}
 */

package ase.awk;

import java.io.*;

/**
 * Extends the core interpreter engine to implement the language closer to 
 * the standard.
 */
public abstract class StdAwk extends Awk
{
	private long seed;
	private java.util.Random random;

	public StdAwk () throws Exception
	{
		super ();

		seed = System.currentTimeMillis();
		random = new java.util.Random (seed);

		addFunction ("sin", 1, 1); 
		addFunction ("cos", 1, 1); 
		addFunction ("tan", 1, 1); 
		addFunction ("atan", 1, 1); 
		addFunction ("atan2", 2, 2); 
		addFunction ("log", 1, 1); 
		addFunction ("exp", 1, 1); 
		addFunction ("sqrt", 1, 1); 
		addFunction ("int", 1, 1, "bfnint"); 

		addFunction ("srand", 0, 1); 
		addFunction ("rand", 0, 0); 

		addFunction ("systime", 0, 0);
		addFunction ("strftime", 0, 2);
		addFunction ("strfgmtime", 0, 2);

		addFunction ("system", 1, 1); 
	}

	/* == file interface == */
	protected int openFile (File file)
	{
		int mode = file.getMode();

		if (mode == File.MODE_READ)
		{
			FileInputStream fis;

			try { fis = new FileInputStream (file.getName()); }
			catch (IOException e) { return -1; }

			Reader rd = new BufferedReader (
				new InputStreamReader (fis)); 
			file.setHandle (rd);
			return 1;
		}
		else if (mode == File.MODE_WRITE)
		{
			FileOutputStream fos;

			try { fos = new FileOutputStream (file.getName()); }
			catch (IOException e) { return -1; }

			Writer wr = new BufferedWriter (
				new OutputStreamWriter (fos));
			file.setHandle (wr);
			return 1;
		}
		else if (mode == File.MODE_APPEND)
		{
			FileOutputStream fos;

			try { fos = new FileOutputStream (file.getName(), true); }
			catch (IOException e) { return -1; }

			Writer wr = new BufferedWriter (
				new OutputStreamWriter (fos));
			file.setHandle (wr);
			return 1;
		}

		return -1;
	}
	
	protected int closeFile (File file)
	{
		int mode = file.getMode();

		if (mode == File.MODE_READ)
		{
			Reader isr = (Reader)file.getHandle();
			try { isr.close (); }
			catch (IOException e) { return -1; }
			return 0;
		}
		else if (mode == File.MODE_WRITE)
		{
			Writer osw = (Writer)file.getHandle();
			try { osw.close (); }
			catch (IOException e) { return -1; }
			return 0;
		}
		else if (mode == File.MODE_APPEND)
		{
			Writer osw = (Writer)file.getHandle();
			try { osw.close (); }
			catch (IOException e) { return -1; }
			return 0;
		}
		
		return -1;
	}

	protected int readFile (File file, char[] buf, int len) 
	{
		int mode = file.getMode();

		if (mode == File.MODE_READ)
		{
			Reader rd = (Reader)file.getHandle();

			try 
			{
				len = rd.read (buf, 0, len);
				if (len == -1) return 0;
			}
			catch (IOException e) { return -1; }
			return len; 
		}

		return -1;
	}

	protected int writeFile (File file, char[] buf, int len) 
	{
		int mode = file.getMode();

		if (mode == File.MODE_WRITE ||
		    mode == File.MODE_APPEND)
		{
			Writer wr = (Writer)file.getHandle();
			try { wr.write (buf, 0, len); }
			catch (IOException e) { return -1; }
			return len;
		}

		return -1;
	}

	protected int flushFile (File file)
	{
		int mode = file.getMode ();

		if (mode == File.MODE_WRITE ||
		    mode == File.MODE_APPEND)
		{
			Writer wr = (Writer)file.getHandle ();
			try { wr.flush (); }
			catch (IOException e) { return -1; }
			return 0;
		}

		return -1;
	}

	private class RWE
	{
		public Writer wr;
		public Reader rd;
		public Reader er;

		public RWE (Writer wr, Reader rd, Reader er)
		{
			this.wr = wr;
			this.rd = rd;
			this.er = er;
		}
	};

	/* == pipe interface == */
	protected int openPipe (Pipe pipe)
	{
		int mode = pipe.getMode();

		if (mode == Pipe.MODE_READ)
		{
			Process proc;
		       
			try { proc = popen (pipe.getName()); }
			catch (IOException e) { return -1; }

			Reader rd = new BufferedReader (
				new InputStreamReader (proc.getInputStream())); 

			pipe.setHandle (rd);
			return 1;
		}
		else if (mode == Pipe.MODE_WRITE)
		{
			Process proc;

			try { proc = popen (pipe.getName()); }
			catch (IOException e) { return -1; }

			Writer wr = new BufferedWriter (
				new OutputStreamWriter (proc.getOutputStream()));
			Reader rd = new BufferedReader (
				new InputStreamReader (proc.getInputStream()));
			Reader er = new BufferedReader (
				new InputStreamReader (proc.getErrorStream()));

			pipe.setHandle (new RWE (wr, rd, er));
			return 1;
		}

		return -1;
	}
	
	protected int closePipe (Pipe pipe)
	{
		int mode = pipe.getMode();

		if (mode == Pipe.MODE_READ)
		{
			Reader rd = (Reader)pipe.getHandle();
			try { rd.close (); }
			catch (IOException e) { return -1; }
			return 0;
		}
		else if (mode == Pipe.MODE_WRITE)
		{
			//Writer wr = (Writer)pipe.getHandle();
			RWE rwe = (RWE)pipe.getHandle();

			try { rwe.wr.close (); }
			catch (IOException e) { return -1; }

			char[] buf = new char[256];

			try 
			{
				while (true)
				{
					int len = rwe.rd.read (buf, 0, buf.length);
					if (len == -1) break;
					System.out.print (new String (buf, 0, len));
				}

				System.out.flush ();
			}
			catch (IOException e) {}

			try
			{
				while (true)
				{
					int len = rwe.er.read (buf, 0, buf.length);
					if (len == -1) break;
					System.err.print (new String (buf, 0, len));
				}

				System.err.flush ();
			}
			catch (IOException e) {}

			try { rwe.rd.close (); } catch (IOException e) {}
			try { rwe.er.close (); } catch (IOException e) {}

			pipe.setHandle (null);
			return 0;
		}
		
		return -1;
	}

	protected int readPipe (Pipe pipe, char[] buf, int len) 
	{
		int mode = pipe.getMode();

		if (mode == Pipe.MODE_READ)
		{
			Reader rd = (Reader)pipe.getHandle();
			try 
			{
				len = rd.read (buf, 0, len);
				if (len == -1) len = 0;
			}
			catch (IOException e) { len = -1; }
			return len; 
		}

		return -1;
	}

	protected int writePipe (Pipe pipe, char[] buf, int len) 
	{
		int mode = pipe.getMode();

		if (mode == Pipe.MODE_WRITE)
		{
			//Writer wr = (Writer)pipe.getHandle ();
			RWE rw = (RWE)pipe.getHandle();
			try 
			{ 
				rw.wr.write (buf, 0, len); 
				rw.wr.flush ();
			}
			catch (IOException e) { return -1; }
			return len;
		}

		return -1;
	}

	protected int flushPipe (Pipe pipe)
	{
		int mode = pipe.getMode ();

		if (mode == Pipe.MODE_WRITE)
		{
			Writer wr = (Writer)pipe.getHandle ();
			try { wr.flush (); }
			catch (IOException e) { return -1; }
			return 0;
		}

		return -1;
	}

	/* == arithmetic built-in functions */
	public void sin (Context ctx, String name, Return ret, Argument[] args) 
	{
		ret.setRealValue (Math.sin(args[0].getRealValue()));
	}

	public void cos (Context ctx, String name, Return ret, Argument[] args)
	{
		ret.setRealValue (Math.cos(args[0].getRealValue()));
	}

	public void tan (Context ctx, String name, Return ret, Argument[] args)
	{
		ret.setRealValue (Math.tan(args[0].getRealValue()));
	}

	public void atan (Context ctx, String name, Return ret, Argument[] args) 
	{
		ret.setRealValue (Math.atan(args[0].getRealValue()));
	}

	public void atan2 (Context ctx, String name, Return ret, Argument[] args) 
	{
		double y = args[0].getRealValue();
		double x = args[1].getRealValue();
		ret.setRealValue (Math.atan2(y,x));
	}

	public void log (Context ctx, String name, Return ret, Argument[] args) 
	{
		ret.setRealValue (Math.log(args[0].getRealValue()));
	}

	public void exp (Context ctx, String name, Return ret, Argument[] args) 
	{
		ret.setRealValue (Math.exp(args[0].getRealValue()));
	}

	public void sqrt (Context ctx, String name, Return ret, Argument[] args) 
	{
		ret.setRealValue (Math.sqrt(args[0].getRealValue()));
	}

	public void bfnint (Context ctx, String name, Return ret, Argument[] args) 
	{
		ret.setIntValue (args[0].getIntValue());
	}

	public void rand (Context ctx, String name, Return ret, Argument[] args)
	{
		ret.setRealValue (random.nextDouble ());
	}

	public void srand (Context ctx, String name, Return ret, Argument[] args) 
	{
		long prev_seed = seed;

		seed = (args == null || args.length == 0)?
			System.currentTimeMillis ():
			args[0].getIntValue();

		random.setSeed (seed);
		ret.setIntValue (prev_seed);
	}

	public void systime (Context ctx, String name, Return ret, Argument[] args) 
	{
		long msec = System.currentTimeMillis ();
		ret.setIntValue (msec / 1000);
	}

	public void strftime (Context ctx, String name, Return ret, Argument[] args) throws Exception
	{
		String fmt = (args.length<1)? "%c": args[0].getStringValue();
		long t = (args.length<2)? (System.currentTimeMillis()/1000): args[1].getIntValue();
		ret.setStringValue (strftime (fmt, t));
	}

	public void strfgmtime (Context ctx, String name, Return ret, Argument[] args) throws Exception
	{
		String fmt = (args.length<1)? "%c": args[0].getStringValue();
		long t = (args.length<2)? (System.currentTimeMillis()/1000): args[1].getIntValue();
		ret.setStringValue (strfgmtime (fmt, t));
	}

	/* miscellaneous built-in functions */
	public void system (Context ctx, String name, Return ret, Argument[] args) throws Exception
	{
		ret.setIntValue (system (args[0].getStringValue()));
	}

	/* == utility functions == */
	private Process popen (String command) throws IOException
	{
		String full;

		/* TODO: consider OS names and versions */
		full = System.getenv ("ComSpec");
		if (full != null)
		{	
			full = full + " /c " + command;
		}
		else
		{
			full = System.getenv ("SHELL");
			if (full != null)
			{
				full = "/bin/sh -c \"" + command + "\"";
			}
			else full = command;
		}

		return Runtime.getRuntime().exec (full);
	}
}