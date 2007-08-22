using System;

using System.Collections.Generic;

using System.ComponentModel;

using System.Data;

using System.Drawing;

using System.Text;

using System.Windows.Forms;

using System.IO;



namespace asetestnet

{

	public partial class AwkForm : Form

	{

		public AwkForm()

		{

			InitializeComponent();

		}



		/*bool sin(string name, ASE.Net.Awk.Argument[] args, ASE.Net.Awk.Return ret)

		{

			//ret.DoubleValue = System.Math.Sin(args[0].RealValue);

			ret.RealValue = System.Math.Sin(args[0].RealValue);

			return true;

		}*/



		public void xxxx()

		{

			System.Windows.Forms.MessageBox.Show ("xxxx - start");

		}

		public void yyyy()

		{

			System.Windows.Forms.MessageBox.Show("yyyy - end");

		}



		public void kkk()

		{

			System.Windows.Forms.MessageBox.Show("zzzz - return");

		}

		public void zzzz()

		{

			System.Windows.Forms.MessageBox.Show("zzzz - statement");

		}

		private void btnRun_Click(object sender, EventArgs e)

		{

			//using (Awk awk = new Awk())

			//for (int i = 0; i < 100; i++)

			{

				Awk awk = new Awk();



				tbxSourceOutput.Text = "";

				tbxConsoleOutput.Text = "";



				//awk.SetWord("BEGIN", "시작");

				//awk.AddFunction("sin", 1, 1, sin);



				//awk.SetMaxDepth(ASE.Net.Awk.DEPTH.BLOCK_RUN, 20);



				awk.OnRunStart += xxxx;

				awk.OnRunEnd += yyyy;

				awk.OnRunReturn += kkk;

				awk.OnRunStatement += zzzz;



				if (!awk.Parse(tbxSourceInput, tbxSourceOutput))

				{

					MessageBox.Show("Parse error");

					//MessageBox.Show(awk.ErrorMessage);

				}

				else

				{



					/*

					awk.EntryPoint = cbxEntryPoint.Text;

					awk.ArgumentsToEntryPoint = chkPassArgumentsToEntryPoint.Checked;*/



					bool n;

					/*int nargs = lbxArguments.Items.Count;

					if (nargs > 0)

					{

						string[] args = new string[nargs];

						for (int i = 0; i < nargs; i++)

							args[i] = lbxArguments.Items[i].ToString();

						n = awk.Run(args);

					}

					else*/

					n = awk.Run(tbxConsoleInput, tbxConsoleOutput);



					if (!n)

					{

						//MessageBox.Show(awk.ErrorMessage);

						MessageBox.Show("Run Error");

					}



				}



				awk.Close();

				//awk.Dispose(); 

			}

			

		}

	

		private void btnAddArgument_Click(object sender, EventArgs e)

		{

			if (tbxArgument.Text.Length > 0)

			{

				lbxArguments.Items.Add(tbxArgument.Text);

				tbxArgument.Text = "";

				tbxArgument.Focus();

			}

		}



		private void btnClearAllArguments_Click(object sender, EventArgs e)

		{

			lbxArguments.Items.Clear();

		}



	}

}

