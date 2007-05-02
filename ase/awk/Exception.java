/*
 * $Id: Exception.java,v 1.3 2007/04/30 05:47:33 bacon Exp $
 *
 * {License}
 */

package ase.awk;

public class Exception extends java.lang.Exception
{
	private int code;
	private int line;

	public Exception () 
	{
		super ();
		this.code = 0;
		this.line = 0;
	}

	public Exception (String msg)
	{
		super (msg);
		this.code = 0;
		this.line = 0;
	}

	public Exception (String msg, int code)
	{
		super (msg);
		this.code = code;
		this.line = 0;
	}

	public Exception (String msg, int code, int line)
	{
		super (msg);
		this.code = code;
		this.line = line;
	}

	public int getCode ()
	{
		return this.code;
	}

	public int getLine ()
	{
		return this.line;
	}
}
