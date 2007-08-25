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



		public void OnAwkRunStart(ASE.Net.Awk awk)

		{

			System.Windows.Forms.MessageBox.Show ("xxxx - start");

		}

		public void OnAwkRunEnd(ASE.Net.Awk awk)

		{

			if (awk.ErrorMessage.Length > 0)

			{

				System.Windows.Forms.MessageBox.Show("Ended with error: " + awk.ErrorMessage);

			}

			else

			{

				System.Windows.Forms.MessageBox.Show("yyyy - end");

			}

		}



		public void kkk(ASE.Net.Awk awk)

		{

			System.Windows.Forms.MessageBox.Show("zzzz - return");

		}

		public void zzzz(ASE.Net.Awk awk)

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



				awk.OnRunStart += OnAwkRunStart;

				awk.OnRunEnd += OnAwkRunEnd;

				awk.OnRunReturn += kkk;

				awk.OnRunStatement += zzzz;



				if (!awk.Parse(tbxSourceInput, tbxSourceOutput))

				{

					MessageBox.Show(awk.ErrorMessage, "Parse Error");

				}

				else

				{

					if (chkPassArgumentsToEntryPoint.Checked)

						awk.Option = awk.Option | ASE.Net.Awk.OPTION.ARGSTOMAIN;

					else

						awk.Option = awk.Option & ~ASE.Net.Awk.OPTION.ARGSTOMAIN;

					

					int nargs = lbxArguments.Items.Count;

					string[] args = null;

					if (nargs > 0)

					{

						args = new string[nargs];

						for (int i = 0; i < nargs; i++)

							args[i] = lbxArguments.Items[i].ToString();

					}

					bool n = awk.Run(tbxConsoleInput, tbxConsoleOutput, cbxEntryPoint.Text, args);

					if (!n)

					{

						MessageBox.Show(awk.ErrorMessage, "Run Error");

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

