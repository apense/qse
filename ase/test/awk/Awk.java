/*
 * $Id: Awk.java,v 1.16 2006-11-29 11:41:15 bacon Exp $
 */

package ase.test.awk;

public class Awk extends ase.awk.StdAwk
{
	private String[] cin;
	private int cin_no;

	private String[] cout;
	private int cout_no;

	public Awk () throws ase.awk.Exception
	{
		super ();

		try { addBuiltinFunction ("sin", 1, 10); } 
		catch (ase.awk.Exception e) { System.out.println (">>>>>> CANNOT ADD sin"); }
		try { addBuiltinFunction ("xxx", 1, 1); } 
		catch (ase.awk.Exception e) { System.out.println (">>>>>> CANNOT ADD xxx"); }
	}

	public Object xxx (Object[] args)
	{
		System.out.println ("BFN_XXX");
		return null;
	}
	public Object sin (Object[] args)
	{
		System.out.println ("<<BFN_SIN>>");
		for (int i = 0; i < args.length; i++)
		{
			System.out.print ("ARG #" + i);
			System.out.print (": ");
			if (args[i] == null) System.out.println ("nil");
			else System.out.println (args[i].toString());
		}
		return null;
	}

	protected String[] getInputConsoleNames ()
	{
		String[] cin = new String[3];
		cin[0] = "c1.txt";
		cin[1] = "c2.txt";
		cin[2] = "c3.txt";
		return cin;
	}	

	protected String[] getOutputConsoleNames ()
	{
		String[] cout = new String[3];
		cout[0] = "c4.txt";
		cout[1] = "c5.txt";
		cout[2] = "";
		return cout;
	}

	protected String[] getSourceNames ()
	{
		String[] cout = new String[1];
		cout[0] = "t.awk";
		return cout;
	}

	/*
	protected String getDeparsedSourceName ()
	{
		return "";
	}
	*/
	protected int getMaxParseDepth ()
	{
		return 50;
	}

	protected int getMaxRunDepth ()
	{
		return 50;
	}

	public static void main (String[] args)
	{
		Awk awk = null;

		try
		{
			awk = new Awk ();
			awk.parse ();
			awk.run ();
		}
		catch (ase.awk.Exception e)
		{
			System.out.println ("ase.awk.Exception - " + e.getMessage());
		}
		finally
		{
			if (awk != null) awk.close ();
		}
	}

}
