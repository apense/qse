
#include <qse/cmn/fs.h>
#include <qse/cmn/stdio.h>
#include <qse/cmn/main.h>

static void list (qse_fs_t* fs, const qse_char_t* name)
{
	qse_fs_ent_t* ent;

	if (qse_fs_chdir (fs, name) <= -1)
	{
		qse_fprintf (QSE_STDERR, QSE_T("Error: Cannot change directory to %s - %s\n"), name, qse_fs_geterrmsg(fs));
		return;
	}	

	qse_printf (QSE_T("----------------------------------------------------------------\n"), fs->curdir);
	qse_printf (QSE_T("CURRENT DIRECTORY: [%s]\n"), fs->curdir);
	qse_printf (QSE_T("----------------------------------------------------------------\n"), fs->curdir);

	do
	{
		qse_btime_t bt;

		ent = qse_fs_read (fs, QSE_FS_ENT_SIZE | QSE_FS_ENT_TYPE | QSE_FS_ENT_TIME);
		if (ent == QSE_NULL) 
		{
			qse_fs_errnum_t e = qse_fs_geterrnum(fs);
			if (e != QSE_FS_ENOERR)
				qse_fprintf (QSE_STDERR, QSE_T("Error: Read error - %s\n"), qse_fs_geterrmsg(fs));
			break;
		}

		qse_localtime (ent->time.modify, &bt);
		qse_printf (QSE_T("%s %16lu %04d-%02d-%02d %02d:%02d %s\n"), 
			((ent->type == QSE_FS_ENT_SUBDIR)? QSE_T("<D>"): QSE_T("   ")),
			(unsigned long)ent->size, 
			bt.year + QSE_BTIME_YEAR_BASE, bt.mon+1, bt.mday, bt.hour, bt.min,
			ent->name.base
		);
	}
	while (1);

}

int fs_main (int argc, qse_char_t* argv[])
{
	qse_fs_t* fs;

	if (argc != 2)
	{
		qse_fprintf (QSE_STDERR, QSE_T("Usage: %s <directory>\n"), argv[0]);
		return -1;
	}

	fs = qse_fs_open (QSE_NULL, 0);
	if (fs == QSE_NULL)
	{
		qse_fprintf (QSE_STDERR, QSE_T("Error: Cannot open directory\n"), argv[1]);
		return -1;
	}

	list (fs, argv[1]);
	list (fs, QSE_T(".."));

	qse_fs_close (fs);
	return 0;
}

int qse_main (int argc, qse_achar_t* argv[])
{
	return qse_runmain (argc, argv, fs_main);
}
