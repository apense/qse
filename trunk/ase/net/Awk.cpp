/*
 * $Id: Awk.cpp,v 1.35 2007/11/08 15:08:06 bacon Exp $
 *
 * {License}
 */

#include "stdafx.h"
#include "Awk.hpp"

#include <ase/utl/ctype.h>
#include <ase/utl/stdio.h>
#include <stdlib.h>
#include <math.h>
#include <tchar.h>

//#include <msclr/auto_gcroot.h>
#include <msclr/gcroot.h>

using System::Runtime::InteropServices::GCHandle;

ASE_BEGIN_NAMESPACE2(ASE,Net)

class MojoAwk: protected ASE::Awk
{
public:
	MojoAwk (): wrapper(nullptr)
	{	
	}

	~MojoAwk ()
	{
	}

	int open (ASE::Net::Awk^ wrapper)
	{
		this->wrapper = wrapper;
		int n = ASE::Awk::open ();
		this->wrapper = nullptr;
		return n;
	}

	void close (ASE::Net::Awk^ wrapper)
	{
		this->wrapper = wrapper;
		Awk::close ();
		this->wrapper = nullptr;	
	}

	int getOption (ASE::Net::Awk^ wrapper) const
	{
		this->wrapper = wrapper;
		int n = ASE::Awk::getOption ();
		this->wrapper = nullptr;
		return n;
	}

	void setOption (ASE::Net::Awk^ wrapper, int opt)
	{
		this->wrapper = wrapper;
		ASE::Awk::setOption (opt);
		this->wrapper = nullptr;
	}

	size_t getErrorLine (ASE::Net::Awk^ wrapper) const
	{
		this->wrapper = wrapper;
		size_t x = ASE::Awk::getErrorLine ();
		this->wrapper = nullptr;
		return x;
	}

	ErrorCode getErrorCode (ASE::Net::Awk^ wrapper) const
	{
		this->wrapper = wrapper;
		ASE::Awk::ErrorCode x = ASE::Awk::getErrorCode ();
		this->wrapper = nullptr;
		return x;
	}

	const char_t* getErrorMessage (ASE::Net::Awk^ wrapper) const
	{
		this->wrapper = wrapper;
		const char_t* x = ASE::Awk::getErrorMessage();
		this->wrapper = nullptr;
		return x;
	}

	const char_t* getErrorString (ASE::Net::Awk^ wrapper, ErrorCode num) const
	{
		this->wrapper = wrapper;
		const char_t* x = ASE::Awk::getErrorString (num);
		this->wrapper = nullptr;
		return x;
	}

	void setError (ASE::Net::Awk^ wrapper, ErrorCode num)
	{
		this->wrapper = wrapper;
		ASE::Awk::setError (num);
		this->wrapper = nullptr;
	}

	void setError (ASE::Net::Awk^ wrapper, ErrorCode num, size_t line)
	{
		this->wrapper = wrapper;
		ASE::Awk::setError (num, line);
		this->wrapper = nullptr;
	}

	void setError (ASE::Net::Awk^ wrapper, ErrorCode num, size_t line, const char_t* arg, size_t len)
	{
		this->wrapper = wrapper;
		ASE::Awk::setError (num, line, arg, len);
		this->wrapper = nullptr;
	}

	void setErrorWithMessage (ASE::Net::Awk^ wrapper, ErrorCode num, size_t line, const char_t* msg)
	{
		this->wrapper = wrapper;
		ASE::Awk::setErrorWithMessage (num, line, msg);
		this->wrapper = nullptr;
	}

	int setErrorString (ASE::Net::Awk^ wrapper, ErrorCode num, const char_t* msg)
	{
		this->wrapper = wrapper;
		int x = ASE::Awk::setErrorString (num, msg);
		this->wrapper = nullptr;
		return x;
	}

	int parse (ASE::Net::Awk^ wrapper)
	{
		this->wrapper = wrapper;
		int n = ASE::Awk::parse ();
		this->wrapper = nullptr;
		return n;
	}

	int run (ASE::Net::Awk^ wrapper, const char_t* main = ASE_NULL, 
	         const char_t** args = ASE_NULL, size_t nargs = 0)
	{
		// run can't be called more than once because this->wrapper 
		// can be set to nullptr while another run is under execution.
		// for the same reason, you can't call other methods except stop
		// untile run ends.
		this->wrapper = wrapper;
		int n = ASE::Awk::run (main, args, nargs);
		this->wrapper = nullptr;
		return n;
	}

	void stop (ASE::Net::Awk^ wrapper)
	{
		if ((ASE::Net::Awk^)this->wrapper != nullptr) ASE::Awk::stop ();
	}

	int getWord (ASE::Net::Awk^ wrapper, const char_t* ow, size_t olen, const char_t** nw, size_t* nlen)
	{
		ASE::Net::Awk^ old = this->wrapper;
		this->wrapper = wrapper;
		int n = ASE::Awk::getWord (ow, olen, nw, nlen);
		this->wrapper = old;
		return n;
	}

	int setWord (ASE::Net::Awk^ wrapper, const char_t* ow, size_t olen, const char_t* nw, size_t nlen)
	{
		ASE::Net::Awk^ old = this->wrapper;
		this->wrapper = wrapper;
		int n = ASE::Awk::setWord (ow, olen, nw, nlen);
		this->wrapper = old;
		return n;
	}

	int unsetWord (ASE::Net::Awk^ wrapper, const char_t* ow, size_t olen)
	{
		ASE::Net::Awk^ old = this->wrapper;
		this->wrapper = wrapper;
		int n = ASE::Awk::unsetWord (ow, olen);
		this->wrapper = old;
		return n;
	}

	int unsetAllWords (ASE::Net::Awk^ wrapper)
	{
		this->wrapper = wrapper;
		int n = ASE::Awk::unsetAllWords ();
		this->wrapper = nullptr;
		return n;
	}

	void setMaxDepth (ASE::Net::Awk^ wrapper, int ids, size_t depth)
	{
		this->wrapper = wrapper;
		ASE::Awk::setMaxDepth (ids, depth);
		this->wrapper = nullptr;
	}

	size_t getMaxDepth (ASE::Net::Awk^ wrapper, int id) const
	{
		this->wrapper = wrapper;
		size_t n = ASE::Awk::getMaxDepth (id);
		this->wrapper = nullptr;
		return n;
	}

	void enableRunCallback (ASE::Net::Awk^ wrapper)
	{
		this->wrapper = wrapper;
		ASE::Awk::enableRunCallback ();
		this->wrapper = nullptr;
	}

	void disableRunCallback (ASE::Net::Awk^ wrapper)
	{
		this->wrapper = wrapper;
		ASE::Awk::disableRunCallback ();
		this->wrapper = nullptr;
	}

	void onRunStart (Run& run)
	{
		wrapper->runErrorReported = false;

		Net::Awk::Context^ ctx = gcnew Net::Awk::Context (wrapper, run);
		GCHandle gh = GCHandle::Alloc (ctx);
		run.setCustom ((void*)GCHandle::ToIntPtr(gh));

		if (wrapper->OnRunStart != nullptr)
		{
			//wrapper->OnRunStart (wrapper);
			try { wrapper->OnRunStart (ctx); }
			catch (...) {}
		}

	}
	void onRunEnd (Run& run)
	{
		System::IntPtr ip ((void*)run.getCustom ());
		GCHandle gh = GCHandle::FromIntPtr (ip);

		ErrorCode code = run.getErrorCode();
		if (code != ERR_NOERR)
		{
			wrapper->runErrorReported = true;
			wrapper->errMsg = gcnew System::String (run.getErrorMessage());
			wrapper->errLine = run.getErrorLine();
			wrapper->errCode = (ASE::Net::Awk::ERROR)code;
		}

		if (wrapper->OnRunEnd != nullptr)
		{
			//wrapper->OnRunEnd (wrapper);
			try { wrapper->OnRunEnd ((ASE::Net::Awk::Context^)gh.Target); }
			catch (...) {}
		}

		gh.Free ();
	}

	void onRunReturn (Run& run, const Argument& ret)
	{
		if (wrapper->OnRunReturn != nullptr)
		{
			System::IntPtr ip ((void*)run.getCustom ());
			GCHandle gh = GCHandle::FromIntPtr (ip);

			//wrapper->OnRunReturn (wrapper);
			try { wrapper->OnRunReturn ((ASE::Net::Awk::Context^)gh.Target); }
			catch (...) {}
		}
	}

	void onRunStatement (Run& run, size_t line)
	{
		if (wrapper->OnRunStatement != nullptr)
		{
			System::IntPtr ip ((void*)run.getCustom ());
			GCHandle gh = GCHandle::FromIntPtr (ip);
	
			//wrapper->OnRunStatement (wrapper);
			try { wrapper->OnRunStatement ((ASE::Net::Awk::Context^)gh.Target); } 
			catch (...) {}
		}
	}

	int addGlobal (ASE::Net::Awk^ wrapper, const char_t* name)
	{
		this->wrapper = wrapper;
		int n = ASE::Awk::addGlobal (name);
		this->wrapper = nullptr;
		return n;
	}

	int deleteGlobal (ASE::Net::Awk^ wrapper, const char_t* name)
	{
		this->wrapper = wrapper;
		int n = ASE::Awk::deleteGlobal (name);
		this->wrapper = nullptr;
		return n;
	}

	int addFunction (
		ASE::Net::Awk^ wrapper,	const char_t* name,
		size_t minArgs, size_t maxArgs, FunctionHandler handler)
	{
		this->wrapper = wrapper;
		int n = ASE::Awk::addFunction (name, minArgs, maxArgs, handler);
		this->wrapper = nullptr;
		return n;
	}

	int deleteFunction (ASE::Net::Awk^ wrapper, const char_t* main)
	{
		this->wrapper = wrapper;
		int n = ASE::Awk::deleteFunction (main);
		this->wrapper = nullptr;
		return n;
	}

	int mojoFunctionHandler (
		Run& run, Return& ret, const Argument* args, size_t nargs, 
		const char_t* name, size_t len)
	{
		System::IntPtr ip ((void*)run.getCustom ());
		GCHandle gh = GCHandle::FromIntPtr (ip);

		return wrapper->DispatchFunction (
			(ASE::Net::Awk::Context^)gh.Target, 
			ret, args, nargs, name, len)? 0: -1;
	}

	int openSource (Source& io) 
	{ 
		ASE::Net::Awk::Source^ nio = gcnew ASE::Net::Awk::Source (
			(ASE::Net::Awk::Source::MODE)io.getMode());

		GCHandle gh = GCHandle::Alloc (nio);
		io.setHandle (GCHandle::ToIntPtr(gh).ToPointer());

		try { return wrapper->OpenSource (nio); }
		catch (...) 
		{ 
			gh.Free ();
			io.setHandle (NULL);
			return -1; 	
		}
	}

	int closeSource (Source& io) 
	{
		System::IntPtr ip ((void*)io.getHandle ());
		GCHandle gh = GCHandle::FromIntPtr (ip);

		try
		{
			return wrapper->CloseSource (
				(ASE::Net::Awk::Source^)gh.Target);
		}
		catch (...) { return -1; }
		finally { gh.Free (); }
	}

	ssize_t readSource (Source& io, char_t* buf, size_t len) 
	{
		System::IntPtr ip ((void*)io.getHandle());
		GCHandle gh = GCHandle::FromIntPtr (ip);

		cli::array<char_t>^ b = nullptr;
		
		try
		{
			b = gcnew cli::array<char_t> (len);
			int n = wrapper->ReadSource (
				(ASE::Net::Awk::Source^)gh.Target, b, len);
			for (int i = 0; i < n; i++) buf[i] = b[i];
			return n;
		}
		catch (...) { return -1; }
		finally { if (b != nullptr) delete b; }
	}

	ssize_t writeSource (Source& io, char_t* buf, size_t len)
	{
		System::IntPtr ip ((void*)io.getHandle());
		GCHandle gh = GCHandle::FromIntPtr (ip);

		cli::array<char_t>^ b = nullptr;
		try
		{
			b = gcnew cli::array<char_t> (len);
			for (size_t i = 0; i < len; i++) b[i] = buf[i];
			return wrapper->WriteSource (
				(ASE::Net::Awk::Source^)gh.Target, b, len);
		}
		catch (...) { return -1; }
		finally { if (b != nullptr) delete b; }
	}

	int openPipe (Pipe& io) 
	{
		ASE::Net::Awk::Pipe^ nio = gcnew ASE::Net::Awk::Pipe (
			gcnew System::String (io.getName ()),
			(ASE::Net::Awk::Pipe::MODE)io.getMode());

		GCHandle gh = GCHandle::Alloc (nio);
		io.setHandle (GCHandle::ToIntPtr(gh).ToPointer());

		try { return wrapper->OpenPipe (nio); }
		catch (...) 
		{ 
			gh.Free ();
			io.setHandle (NULL);
			return -1; 	
		}
	}

	int closePipe (Pipe& io) 
	{
		System::IntPtr ip ((void*)io.getHandle ());
		GCHandle gh = GCHandle::FromIntPtr (ip);

		try
		{
			return wrapper->ClosePipe (
				(ASE::Net::Awk::Pipe^)gh.Target);
		}
		catch (...) { return -1; }
		finally { gh.Free (); }
	}

	ssize_t readPipe  (Pipe& io, char_t* buf, size_t len) 
	{
		System::IntPtr ip ((void*)io.getHandle());
		GCHandle gh = GCHandle::FromIntPtr (ip);

		cli::array<char_t>^ b = nullptr;
		
		try
		{
			b = gcnew cli::array<char_t> (len);
			int n = wrapper->ReadPipe (
				(ASE::Net::Awk::Pipe^)gh.Target, b, len);
			for (int i = 0; i < n; i++) buf[i] = b[i];
			return n;
		}
		catch (...) { return -1; }
		finally { if (b != nullptr) delete b; }
	}

	ssize_t writePipe (Pipe& io, char_t* buf, size_t len) 
	{
		System::IntPtr ip ((void*)io.getHandle());
		GCHandle gh = GCHandle::FromIntPtr (ip);

		cli::array<char_t>^ b = nullptr;
		try
		{
			b = gcnew cli::array<char_t> (len);
			for (size_t i = 0; i < len; i++) b[i] = buf[i];
			return wrapper->WritePipe (
				(ASE::Net::Awk::Pipe^)gh.Target, b, len);
		}
		catch (...) { return -1; }
		finally { if (b != nullptr) delete b; }
	}

	int flushPipe (Pipe& io) 
	{
		System::IntPtr ip ((void*)io.getHandle());
		GCHandle gh = GCHandle::FromIntPtr (ip);

		try
		{
			return wrapper->FlushPipe (
				(ASE::Net::Awk::Pipe^)gh.Target);
		}
		catch (...) { return -1; }
	}

	int openFile (File& io) 
	{	
		ASE::Net::Awk::File^ nio = gcnew ASE::Net::Awk::File (
			gcnew System::String (io.getName ()),
			(ASE::Net::Awk::File::MODE)io.getMode());

		GCHandle gh = GCHandle::Alloc (nio);
		io.setHandle (GCHandle::ToIntPtr(gh).ToPointer());

		try { return wrapper->OpenFile (nio); }
		catch (...)
		{
			gh.Free ();
			io.setHandle (NULL);
			return -1;
		}
	}

	int closeFile (File& io) 
	{
		System::IntPtr ip ((void*)io.getHandle ());
		GCHandle gh = GCHandle::FromIntPtr (ip);

		try
		{
			return wrapper->CloseFile (
				(ASE::Net::Awk::File^)gh.Target);
		}
		catch (...) { return -1; }
		finally { gh.Free (); }
	}

	ssize_t readFile (File& io, char_t* buf, size_t len) 
	{
		System::IntPtr ip ((void*)io.getHandle());
		GCHandle gh = GCHandle::FromIntPtr (ip);

		cli::array<char_t>^ b = nullptr;
		try
		{
			b = gcnew cli::array<char_t> (len);
			int n = wrapper->ReadFile (
				(ASE::Net::Awk::File^)gh.Target, b, len);
			for (int i = 0; i < n; i++) buf[i] = b[i];
			return n;
		}
		catch (...) { return -1; }
		finally { if (b != nullptr) delete b; }
	}

	ssize_t writeFile (File& io, char_t* buf, size_t len) 
	{
		System::IntPtr ip ((void*)io.getHandle());
		GCHandle gh = GCHandle::FromIntPtr (ip);

		cli::array<char_t>^ b = nullptr;
		try
		{
			b = gcnew cli::array<char_t> (len);
			for (size_t i = 0; i < len; i++) b[i] = buf[i];
			return wrapper->WriteFile (
				(ASE::Net::Awk::File^)gh.Target, b, len);
		}
		catch (...) { return -1; }
		finally { if (b != nullptr) delete b; }
	}

	int flushFile (File& io) 
	{
		System::IntPtr ip ((void*)io.getHandle());
		GCHandle gh = GCHandle::FromIntPtr (ip);

		try
		{
			return wrapper->FlushFile (
				(ASE::Net::Awk::File^)gh.Target);
		}
		catch (...) { return -1; }
	}

	int openConsole (Console& io) 
	{	
		ASE::Net::Awk::Console^ nio = gcnew ASE::Net::Awk::Console (
			gcnew System::String (io.getName ()),
			(ASE::Net::Awk::Console::MODE)io.getMode());

		GCHandle gh = GCHandle::Alloc (nio);
		io.setHandle (GCHandle::ToIntPtr(gh).ToPointer());

		try { return wrapper->OpenConsole (nio); }
		catch (...)
		{
			gh.Free ();
			io.setHandle (NULL);
			return -1;
		}
	}

	int closeConsole (Console& io) 
	{
		System::IntPtr ip ((void*)io.getHandle ());
		GCHandle gh = GCHandle::FromIntPtr (ip);

		try
		{
			return wrapper->CloseConsole (
				(ASE::Net::Awk::Console^)gh.Target);
		}
		catch (...) { return -1; }
		finally { gh.Free (); }
	}

	ssize_t readConsole (Console& io, char_t* buf, size_t len) 
	{
		System::IntPtr ip ((void*)io.getHandle());
		GCHandle gh = GCHandle::FromIntPtr (ip);

		cli::array<char_t>^ b = nullptr;
		try
		{
			b = gcnew cli::array<char_t> (len);
			int n = wrapper->ReadConsole (
				(ASE::Net::Awk::Console^)gh.Target, b, len);
			for (int i = 0; i < n; i++) buf[i] = b[i];
			return n;
		}
		catch (...) { return -1; }
		finally { if (b != nullptr) delete b; }
	}

	ssize_t writeConsole (Console& io, char_t* buf, size_t len) 
	{
		System::IntPtr ip ((void*)io.getHandle());
		GCHandle gh = GCHandle::FromIntPtr (ip);

		cli::array<char_t>^ b = nullptr;
		try
		{
			b = gcnew cli::array<char_t> (len);
			for (size_t i = 0; i < len; i++) b[i] = buf[i];
			return wrapper->WriteConsole (
				(ASE::Net::Awk::Console^)gh.Target, b, len);
		}
		catch (...) { return -1; }
		finally { if (b != nullptr) delete b; }
	}

	int flushConsole (Console& io) 
	{
		System::IntPtr ip ((void*)io.getHandle());
		GCHandle gh = GCHandle::FromIntPtr (ip);

		try
		{
			return wrapper->FlushConsole (
				(ASE::Net::Awk::Console^)gh.Target);
		}
		catch (...) { return -1; }
	}

	int nextConsole (Console& io) 
	{
		System::IntPtr ip ((void*)io.getHandle());
		GCHandle gh = GCHandle::FromIntPtr (ip);

		try
		{
			return wrapper->NextConsole (
				(ASE::Net::Awk::Console^)gh.Target);
		}
		catch (...) { return -1; }
	}

	// primitive operations 
	void* allocMem   (size_t n) { return ::malloc (n); }
	void* reallocMem (void* ptr, size_t n) { return ::realloc (ptr, n); }
	void  freeMem    (void* ptr) { ::free (ptr); }

	bool_t isUpper  (cint_t c) { return ase_isupper (c); }
	bool_t isLower  (cint_t c) { return ase_islower (c); }
	bool_t isAlpha  (cint_t c) { return ase_isalpha (c); }
	bool_t isDigit  (cint_t c) { return ase_isdigit (c); }
	bool_t isXdigit (cint_t c) { return ase_isxdigit (c); }
	bool_t isAlnum  (cint_t c) { return ase_isalnum (c); }
	bool_t isSpace  (cint_t c) { return ase_isspace (c); }
	bool_t isPrint  (cint_t c) { return ase_isprint (c); }
	bool_t isGraph  (cint_t c) { return ase_isgraph (c); }
	bool_t isCntrl  (cint_t c) { return ase_iscntrl (c); }
	bool_t isPunct  (cint_t c) { return ase_ispunct (c); }
	cint_t toUpper  (cint_t c) { return ase_toupper (c); }
	cint_t toLower  (cint_t c) { return ase_tolower (c); }

	real_t pow (real_t x, real_t y) 
	{ 
		return ::pow (x, y); 
	}

	int vsprintf (char_t* buf, size_t size, const char_t* fmt, va_list arg) 
	{
		return ase_vsprintf (buf, size, fmt, arg);
	}

	void vdprintf (const char_t* fmt, va_list arg) 
	{
		ase_vfprintf (stderr, fmt, arg);
	}

protected:
	//msclr::auto_gcroot<ASE::Net::Awk^> wrapper;		
	mutable gcroot<ASE::Net::Awk^> wrapper;		
};

Awk::Awk ()
{
	funcs = gcnew System::Collections::Hashtable();

	awk = new ASE::Net::MojoAwk ();
	if (awk->open (this) == -1)
	{
		throw gcnew System::Exception (gcnew System::String(awk->getErrorMessage(this)));				
	}

	//option = (OPTION)(awk->getOption (this) | MojoAwk::OPT_CRLF);	
	option = (OPTION)(awk->getOption (this) | ASE::Awk::OPT_CRLF);
	awk->setOption (this, (int)option);

	errMsg = "";
	errLine = 0;
	errCode = ASE::Net::Awk::ERROR::NOERR;
	runErrorReported = false;
}

Awk::~Awk ()
{
	if (awk != NULL)
	{
		awk->close (this);
		delete awk;
		awk = NULL;
	}

	if (funcs != nullptr)
	{
		funcs->Clear ();
		delete funcs;
		funcs = nullptr;
	}
}

Awk::!Awk ()
{
	if (awk != NULL)
	{
		awk->close (this);
		delete awk;
		awk = NULL;
	}
}

Awk::OPTION Awk::Option::get ()
{
	if (awk != NULL) this->option = (OPTION)awk->getOption (this);
	return this->option; 
}

void Awk::Option::set (Awk::OPTION opt)
{
	this->option = opt;
	if (awk != NULL) awk->setOption (this, (int)this->option);
}

bool Awk::SetErrorString (Awk::ERROR num, System::String^ msg)
{
	if (awk == NULL) 
	{
		SetError (ERROR::NOPER);
		return false;
	}

	cli::pin_ptr<const ASE::Awk::char_t> nptr = PtrToStringChars(msg);
	bool r = (awk->setErrorString (this, (ASE::Awk::ErrorCode)num, nptr) == 0);
	if (!r) { RetrieveError (); }
	return r;
}

void Awk::Close ()
{
	if (awk != NULL) 
	{
		awk->close (this);
		delete awk;
		awk = NULL;
	}

	if (funcs != nullptr)
	{
		funcs->Clear ();
		delete funcs;
		funcs = nullptr;
	}
}

bool Awk::Parse ()
{
	if (awk == NULL) 
	{
		SetError (ERROR::NOPER);
		return false;
	}
	bool r = (awk->parse (this) == 0);
	if (!r) { RetrieveError (); }
	return r;
}

bool Awk::Run ()
{
	return Run (nullptr, nullptr);
}

bool Awk::Run (System::String^ entryPoint, cli::array<System::String^>^ args)
{
	runErrorReported = false;

	if (awk == NULL) 
	{
		SetError (ERROR::NOPER);
		return false;
	}

	//if (OnRunStart != nullptr || OnRunEnd != nullptr || 
	//    OnRunReturn != nullptr || OnRunStatement != nullptr)
	//{
		awk->enableRunCallback (this);
	//}

	if (args == nullptr || args->Length <= 0)
	{
		if (entryPoint == nullptr || entryPoint->Length <= 0)
		{
			bool r = (awk->run (this) == 0);
			if (runErrorReported) r = false;
			else if (!r) RetrieveError ();
			return r;
		}
		else
		{
			cli::pin_ptr<const ASE::Awk::char_t> nptr = PtrToStringChars(entryPoint);
			bool r = (awk->run (this, nptr) == 0);
			if (runErrorReported) r = false;
			else if (!r) RetrieveError ();
			return r;
		}
	}
	else
	{
		int nargs = args->Length;					
		ASE::Awk::char_t** ptr = ASE_NULL;

		try
		{
			bool r = false;

			ptr = (ASE::Awk::char_t**)awk->allocMem (nargs * ASE_SIZEOF(ASE::Awk::char_t*));
			if (ptr == ASE_NULL)
			{
				SetError (ERROR::NOMEM);
				return false;
			}
			for (int i = 0; i < nargs; i++) ptr[i] = ASE_NULL;
			for (int i = 0; i < nargs; i++)
			{
				cli::pin_ptr<const ASE::Awk::char_t> nptr = PtrToStringChars (args[i]);
				ptr[i] = (ASE::Awk::char_t*)awk->allocMem ((args[i]->Length+1)*ASE_SIZEOF(ASE::Awk::char_t));
				if (ptr[i] == ASE_NULL)
				{
					r = false;
					SetError (ERROR::NOMEM);
					goto exit_run;
				}
				memcpy (ptr[i], nptr, args[i]->Length*ASE_SIZEOF(ASE::Awk::char_t));
				ptr[i][args[i]->Length] = ASE_T('\0');
			}

			if (entryPoint == nullptr || entryPoint->Length <= 0)
			{
				r = (awk->run (this, ASE_NULL, (const ASE::Awk::char_t**)ptr, nargs) == 0);
			}
			else
			{
				cli::pin_ptr<const ASE::Awk::char_t> nptr = PtrToStringChars(entryPoint);
				r = (awk->run (this, nptr, (const ASE::Awk::char_t**)ptr, nargs) == 0);
			}

		exit_run:
			if (ptr != ASE_NULL)
			{
				for (int i = 0; i < nargs; i++) 
				{
					if (ptr[i] != ASE_NULL) 
					{
						awk->freeMem (ptr[i]);
						ptr[i] = ASE_NULL;
					}
				}

				awk->freeMem (ptr);
				ptr = ASE_NULL;
			}
		
			if (runErrorReported) r = false;
			else if (!r) RetrieveError ();
			
			return r;
		}
		catch (...)
		{
			if (ptr != ASE_NULL)
			{
				for (int i = 0; i < nargs; i++) 
				{
					if (ptr[i] != ASE_NULL) 
					{
						awk->freeMem (ptr[i]);
						ptr[i] = ASE_NULL;
					}
				}
				awk->freeMem (ptr);
				ptr = ASE_NULL;
			}

			SetError (ERROR::NOMEM);
			return false;
		}
	}
}

bool Awk::Stop ()
{
	if (awk == NULL) 
	{
		SetError (ERROR::NOPER);
		return false;
	}

	awk->stop (this);
	return true;
}

bool Awk::AddGlobal (System::String^ name, [System::Runtime::InteropServices::Out] int% id)
{
	if (awk == NULL) 
	{
		SetError (ERROR::NOPER);
		return false;
	}

	cli::pin_ptr<const ASE::Awk::char_t> nptr = PtrToStringChars(name);
	int n = awk->addGlobal (this, nptr);
	if (n == -1) 
	{
		RetrieveError ();
		return false;
	}

	id = n;
	return true;
}

bool Awk::DeleteGlobal (System::String^ name)
{
	if (awk == NULL) 
	{
		SetError (ERROR::NOPER);
		return false;
	}

	cli::pin_ptr<const ASE::Awk::char_t> nptr = PtrToStringChars(name);
	int n = awk->deleteGlobal (this, nptr);
	if (n == -1) RetrieveError ();
	return n == 0;
}

bool Awk::AddFunction (
	System::String^ name, int minArgs, int maxArgs, 
	FunctionHandler^ handler)
{
	if (awk == NULL) 
	{
		SetError (ERROR::NOPER);
		return false;
	}
	cli::pin_ptr<const ASE::Awk::char_t> nptr = PtrToStringChars(name);
	int n = awk->addFunction (this, nptr, minArgs, maxArgs, 
		(ASE::Awk::FunctionHandler)&MojoAwk::mojoFunctionHandler);
	if (n == 0) funcs->Add(name, handler);	
	else RetrieveError ();
	return n == 0;
}

bool Awk::DeleteFunction (System::String^ name)
{
	if (awk == NULL) 
	{
		SetError (ERROR::NOPER);
		return false;
	}
	cli::pin_ptr<const ASE::Awk::char_t> nptr = PtrToStringChars(name);
	int n = awk->deleteFunction (this, nptr);
	if (n == 0) funcs->Remove (name);
	else RetrieveError ();
	return n == 0;
}

bool Awk::DispatchFunction (
	Context^ ctx, ASE::Awk::Return& ret, 
	const ASE::Awk::Argument* args, size_t nargs, 
	const char_t* name, size_t len)
{
	System::String^ nm = gcnew System::String (name, 0, len);

	FunctionHandler^ fh = (FunctionHandler^)funcs[nm];
	if (fh == nullptr) 
	{
		ctx->SetError (ERROR::INVAL);
		return false;
	}
	
	Return^ r = gcnew Return (ret);
	cli::array<Argument^>^ a = gcnew cli::array<Argument^> (nargs);

	size_t i;
	for (i = 0; i < nargs; i++) 
		a[i] = gcnew Argument(ctx, args[i]);

	bool n = fh (ctx, nm, a, r);

	while (i > 0) delete a[--i];
	delete a;
	delete r;

	return n;
}

System::String^ Awk::GetWord (System::String^ ow)
{
	if (awk == NULL) 
	{
		SetError (ERROR::NOPER);
		return nullptr;
	}

	const char_t* nptr;
	size_t nlen;

	cli::pin_ptr<const ASE::Awk::char_t> optr = PtrToStringChars(ow);
	if (awk->getWord (this, optr, ow->Length, &nptr, &nlen) == -1)
	{
		RetrieveError ();
		return nullptr;
	}

	return gcnew System::String (nptr, 0, nlen);
}

bool Awk::SetWord (System::String^ ow, System::String^ nw)
{
	if (awk == NULL) 
	{
		SetError (ERROR::NOPER);
		return false;
	}

	cli::pin_ptr<const ASE::Awk::char_t> optr = PtrToStringChars(ow);
	cli::pin_ptr<const ASE::Awk::char_t> nptr = PtrToStringChars(nw);
	return (awk->setWord (this, optr, ow->Length, nptr, nw->Length) == 0);
}

bool Awk::UnsetWord (System::String^ ow)
{
	if (awk == NULL) 
	{
		SetError (ERROR::NOPER);
		return false;
	}
	cli::pin_ptr<const ASE::Awk::char_t> optr = PtrToStringChars(ow);
	return (awk->unsetWord (this, optr, ow->Length) == 0);
}

bool Awk::UnsetAllWords ()
{
	if (awk == NULL) 
	{
		SetError (ERROR::NOPER);
		return false;
	}
	return (awk->unsetAllWords (this) == 0);
}

bool Awk::SetMaxDepth (DEPTH id, size_t depth)
{
	if (awk == NULL) 
	{
		SetError (ERROR::NOPER);
		return false;
	}
	awk->setMaxDepth (this, (int)id, depth);
	return true;
}

bool Awk::GetMaxDepth (DEPTH id, size_t* depth)
{
	if (awk == NULL) 
	{
		SetError (ERROR::NOPER);
		return false;
	}
	*depth = awk->getMaxDepth (this, (int)id);
	return true;
}

void Awk::SetError (ERROR num)
{
	if (awk != NULL)
	{
		awk->setError (this, (ASE::Awk::ErrorCode)num);
		RetrieveError ();
	}
	else
	{
		errMsg = "";
		errLine = 0;
		errCode = num;
	}
}

void Awk::SetError (ERROR num, size_t line)
{
	if (awk != NULL)
	{
		awk->setError (this, (ASE::Awk::ErrorCode)num, line);
		RetrieveError ();
	}
	else
	{
		errMsg = "";
		errLine = line;
		errCode = num;
	}
}

void Awk::SetError (ERROR num, size_t line, System::String^ arg)
{
	if (awk != NULL)
	{
		cli::pin_ptr<const ASE::Awk::char_t> p = PtrToStringChars(arg);
		awk->setError (this, (ASE::Awk::ErrorCode)num, line, p, arg->Length);
		RetrieveError ();
	}
	else
	{
		errMsg = "";
		errLine = line;
		errCode = num;
	}
}

void Awk::SetErrorWithMessage (ERROR num, size_t line, System::String^ msg)
{
	if (awk != NULL)
	{
		cli::pin_ptr<const ASE::Awk::char_t> p = PtrToStringChars(msg);
		awk->setErrorWithMessage (this, (ASE::Awk::ErrorCode)num, line, p);
		RetrieveError ();
	}
	else
	{
		errMsg = msg;
		errLine = line;
		errCode = num;
	}
}

void Awk::RetrieveError ()
{
	if (awk != NULL)
	{
		errMsg = gcnew System::String (awk->getErrorMessage(this));
		errLine = awk->getErrorLine (this);
		errCode = (ERROR)awk->getErrorCode (this);
	}
}

ASE_END_NAMESPACE2(Net,ASE)
