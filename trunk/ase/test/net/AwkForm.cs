using System;

using System.Collections.Generic;

using System.ComponentModel;

using System.Data;

using System.Drawing;

using System.Text;

using System.Windows.Forms;

using System.IO;



namespace ase.net

{

	public partial class AwkForm : Form

	{

		protected System.Threading.Thread thr = null;

		protected Awk awk = null;

			

		public AwkForm()

		{

			InitializeComponent();

			StartingRunHandlers += StartingRun;

			EndingRunHandlers += EndingRun;

			ShowMessageHandlers += ShowMessage;

		}



		public delegate void StartingRunHandler();

		public delegate void EndingRunHandler();

		public delegate void ShowMessageHandler(string msg, string cap);



		public event StartingRunHandler StartingRunHandlers;

		public event EndingRunHandler EndingRunHandlers;

		public event ShowMessageHandler ShowMessageHandlers;



		private void StartingRun()

		{

			tbxSourceOutput.Text = "";

			tbxConsoleOutput.Text = "";

			btnRun.Text = "Stop";

		}



		private void EndingRun()

		{

			btnRun.Text = "Run";

			awk = null;

		}



		private void ShowMessage(string msg, string cap)

		{

			MessageBox.Show(msg, cap, MessageBoxButtons.OK, MessageBoxIcon.Error);

		}



		public void OnAwkRunStart(ASE.Net.Awk awk)

		{

			System.Windows.Forms.MessageBox.Show ("OnAwkRunStart");

		}

		public void OnAwkRunEnd(ASE.Net.Awk awk)

		{

			/*if (awk.ErrorMessage.Length > 0)

			{

				System.Windows.Forms.MessageBox.Show("Ended with error: " + awk.ErrorMessage);

			}

			else

			{

				System.Windows.Forms.MessageBox.Show("Ended Successfully");

			}*/

		}



		public void OnAwkReturn (ASE.Net.Awk awk)

		{

			System.Windows.Forms.MessageBox.Show("OnAwkReturn");

		}

		public void OnAwkStatement (ASE.Net.Awk awk)

		{

			System.Windows.Forms.MessageBox.Show("OnAwkStatement");

		}

		

		private void btnRun_Click(object sender, EventArgs e)

		{

			if (awk == null)

			{

				thr = new System.Threading.Thread(

					new System.Threading.ParameterizedThreadStart(AwkService));

				thr.Start(new object[] { this, cbxEntryPoint.Text });

			}

			else

			{

				awk.Stop();

			}

	

			//AwkService(new object[] { null, cbxEntryPoint.Text });

		}



		private void AwkService (object arg)

		{

			object[] argarr = (object[])arg;

			System.ComponentModel.ISynchronizeInvoke si = (System.ComponentModel.ISynchronizeInvoke)argarr[0];

			string entryPoint = (string)argarr[1];



			//using (Awk awk = new Awk(si))

			//for (int x = 0; x < 100000; x++)

			{

				awk = new Awk(si);



				if (si != null && si.InvokeRequired)

					si.Invoke(StartingRunHandlers, null);

				else StartingRun();

				

				//awk.SetWord("BEGIN", "시작");

				//awk.SetWord("sleep", "cleep");

				//awk.SetErrorString(ASE.Net.Awk.ERROR.LBRACE, "'%.*s' 자리에 '{'가 필요합니다");

				//awk.SetMaxDepth(ASE.Net.Awk.DEPTH.BLOCK_RUN, 20);



				//awk.OnRunStart += OnAwkRunStart;

				//awk.OnRunEnd += OnAwkRunEnd;

				//awk.OnRunReturn += OnAwkReturn;

				//awk.OnRunStatement += OnAwkStatement;



				foreach (object item in clbOptions.Items)

				{

					Option opt = (Option)item;

					if (clbOptions.GetItemCheckState(clbOptions.Items.IndexOf(item)).Equals (CheckState.Checked))

					{

						awk.Option = awk.Option | opt.Value;

					}

					else

					{

						awk.Option = awk.Option & ~opt.Value;

					}

				}



				

				if (!awk.Parse(tbxSourceInput, tbxSourceOutput))

				{

					string msg;

					

					if (awk.ErrorLine == 0) msg = awk.ErrorMessage;

					else msg = awk.ErrorMessage + " at line " + awk.ErrorLine;





					if (si != null && si.InvokeRequired)

						si.Invoke(ShowMessageHandlers, new object[] {msg, "Parse Error"});

					else ShowMessage(msg, "Parse Error");

				}

				else

				{	

					int nargs = lbxArguments.Items.Count;

					string[] args = null;

					if (nargs > 0)

					{

						args = new string[nargs];

						for (int i = 0; i < nargs; i++)

							args[i] = lbxArguments.Items[i].ToString();

					}

					

					bool n = awk.Run(tbxConsoleInput, tbxConsoleOutput, entryPoint, args);

					if (!n)

					{

						string msg;



						if (awk.ErrorLine == 0) msg = awk.ErrorMessage;

						else msg = awk.ErrorMessage + " at line " + awk.ErrorLine;



						if (si != null && si.InvokeRequired)

							si.Invoke(ShowMessageHandlers, new object[] {msg, "Run Error"});

						else ShowMessage(msg, "Run Error");

					}

				}

				

				awk.Close();



				if (si != null && si.InvokeRequired)

					si.Invoke(EndingRunHandlers, null);

				else EndingRun();

				//awk.Dispose(); 		

			}



			//System.GC.Collect();

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



		class Option

		{

			private string name;

			private ASE.Net.Awk.OPTION value;



			public Option(string name, ASE.Net.Awk.OPTION value)

			{

				this.name = name;

				this.value = value;

			}



			public string Name

			{

				get { return this.name; }

			}



			public ASE.Net.Awk.OPTION Value

			{

				get { return this.value; }

			}



			public override string ToString()

			{

				return this.name;

			}

		}



		private void AwkForm_Load(object sender, EventArgs e)

		{

			clbOptions.Items.Add(new Option("IMPLICIT", ASE.Net.Awk.OPTION.IMPLICIT), true);

			clbOptions.Items.Add(new Option("EXPLICIT", ASE.Net.Awk.OPTION.EXPLICIT));

			clbOptions.Items.Add(new Option("UNIQUEFN", ASE.Net.Awk.OPTION.UNIQUEFN), true);

			clbOptions.Items.Add(new Option("SHADING", ASE.Net.Awk.OPTION.SHADING), true);

			clbOptions.Items.Add(new Option("SHIFT", ASE.Net.Awk.OPTION.SHIFT));

			clbOptions.Items.Add(new Option("IDIV", ASE.Net.Awk.OPTION.IDIV));

			clbOptions.Items.Add(new Option("STRCONCAT", ASE.Net.Awk.OPTION.STRCONCAT));

			clbOptions.Items.Add(new Option("EXTIO", ASE.Net.Awk.OPTION.EXTIO), true);

			clbOptions.Items.Add(new Option("BLOCKLESS", ASE.Net.Awk.OPTION.BLOCKLESS), true);

			clbOptions.Items.Add(new Option("BASEONE", ASE.Net.Awk.OPTION.BASEONE), true);

			clbOptions.Items.Add(new Option("STRIPSPACES", ASE.Net.Awk.OPTION.STRIPSPACES));

			clbOptions.Items.Add(new Option("NEXTOFILE", ASE.Net.Awk.OPTION.NEXTOFILE));

			clbOptions.Items.Add(new Option("CRLF", ASE.Net.Awk.OPTION.CRLF), true);

			clbOptions.Items.Add(new Option("ARGSTOMAIN", ASE.Net.Awk.OPTION.ARGSTOMAIN));

			clbOptions.Items.Add(new Option("RESET", ASE.Net.Awk.OPTION.RESET));

			clbOptions.Items.Add(new Option("MAPTOVAR", ASE.Net.Awk.OPTION.MAPTOVAR));

			clbOptions.Items.Add(new Option("PABLOCK", ASE.Net.Awk.OPTION.PABLOCK), true);

		}



	}

}

