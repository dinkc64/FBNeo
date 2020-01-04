#include "burner.h"
#include "directx9_core.h"
// Direct3DCreate9
LPDIRECT3D9 (WINAPI* _Direct3DCreate9)(UINT);
LPDIRECT3D9 WINAPI Empty_Direct3DCreate9(UINT) { return NULL; }

// D3DXFillTextureTX
HRESULT (WINAPI* _D3DXFillTextureTX)(LPDIRECT3DTEXTURE9, LPD3DXTEXTURESHADER);
HRESULT WINAPI Empty_D3DXFillTextureTX(LPDIRECT3DTEXTURE9, LPD3DXTEXTURESHADER) { return 0; }

// D3DXCreateBuffer
HRESULT (WINAPI* _D3DXCreateBuffer)(DWORD, LPD3DXBUFFER*);
HRESULT WINAPI Empty_D3DXCreateBuffer(DWORD, LPD3DXBUFFER*) { return 0; }

// D3DXCreateEffectFromResource
#ifdef UNICODE
HRESULT (WINAPI* _D3DXCreateEffectFromResource)(LPDIRECT3DDEVICE9, HMODULE, LPCWSTR, const D3DXMACRO*, LPD3DXINCLUDE,
                                                DWORD, LPD3DXEFFECTPOOL, LPD3DXEFFECT*, LPD3DXBUFFER*);

HRESULT WINAPI Empty_D3DXCreateEffectFromResource(LPDIRECT3DDEVICE9, HMODULE, LPCWSTR, const D3DXMACRO*, LPD3DXINCLUDE,
                                                  DWORD, LPD3DXEFFECTPOOL, LPD3DXEFFECT*, LPD3DXBUFFER*) { return 0; }
#else
HRESULT (WINAPI* _D3DXCreateEffectFromResource) (LPDIRECT3DDEVICE9,HMODULE,LPCTSTR,const D3DXMACRO*,LPD3DXINCLUDE,DWORD,LPD3DXEFFECTPOOL,LPD3DXEFFECT*,LPD3DXBUFFER*);
HRESULT WINAPI Empty_D3DXCreateEffectFromResource (LPDIRECT3DDEVICE9,HMODULE,LPCTSTR,const D3DXMACRO*,LPD3DXINCLUDE,DWORD,LPD3DXEFFECTPOOL,LPD3DXEFFECT*,LPD3DXBUFFER*) { return 0; }
#endif

// D3DXCreateEffectFromFile
#ifdef UNICODE
HRESULT (WINAPI* _D3DXCreateEffectFromFile)(LPDIRECT3DDEVICE9, LPCWSTR, const D3DXMACRO*, LPD3DXINCLUDE, DWORD,
                                            LPD3DXEFFECTPOOL, LPD3DXEFFECT*, LPD3DXBUFFER*);

HRESULT WINAPI Empty_D3DXCreateEffectFromFile(LPDIRECT3DDEVICE9, LPCWSTR, const D3DXMACRO*, LPD3DXINCLUDE, DWORD,
                                              LPD3DXEFFECTPOOL, LPD3DXEFFECT*, LPD3DXBUFFER*) { return 0; }
#else
HRESULT (WINAPI* _D3DXCreateEffectFromFile) (LPDIRECT3DDEVICE9,LPCTSTR,const D3DXMACRO*,LPD3DXINCLUDE,DWORD,LPD3DXEFFECTPOOL,LPD3DXEFFECT*,LPD3DXBUFFER*);
HRESULT WINAPI Empty_D3DXCreateEffectFromFile (LPDIRECT3DDEVICE9,LPCTSTR,const D3DXMACRO*,LPD3DXINCLUDE,DWORD,LPD3DXEFFECTPOOL,LPD3DXEFFECT*,LPD3DXBUFFER*) { return 0; }
#endif

// D3DXLoadSurfaceFromMemory
HRESULT (WINAPI* _D3DXLoadSurfaceFromMemory)(LPDIRECT3DSURFACE9, const PALETTEENTRY*, const RECT*, LPCVOID, D3DFORMAT,
                                             UINT, const PALETTEENTRY*, const RECT*, DWORD, D3DCOLOR);

HRESULT WINAPI Empty_D3DXLoadSurfaceFromMemory(LPDIRECT3DSURFACE9, const PALETTEENTRY*, const RECT*, LPCVOID, D3DFORMAT,
                                               UINT, const PALETTEENTRY*, const RECT*, DWORD, D3DCOLOR) { return 0; }

// D3DXCompileShaderFromResource
#ifdef UNICODE
HRESULT (WINAPI* _D3DXCompileShaderFromResource)(HMODULE, LPCWSTR, const D3DXMACRO*, LPD3DXINCLUDE, LPCSTR, LPCSTR,
                                                 DWORD, LPD3DXBUFFER*, LPD3DXBUFFER*, LPD3DXCONSTANTTABLE*);

HRESULT WINAPI Empty_D3DXCompileShaderFromResource(HMODULE, LPCWSTR, const D3DXMACRO*, LPD3DXINCLUDE, LPCSTR, LPCSTR,
                                                   DWORD, LPD3DXBUFFER*, LPD3DXBUFFER*, LPD3DXCONSTANTTABLE*)
{
	return 0;
}
#else
HRESULT (WINAPI* _D3DXCompileShaderFromResource) (HMODULE,LPCTSTR,const D3DXMACRO*,LPD3DXINCLUDE,LPCSTR,LPCSTR,DWORD,LPD3DXBUFFER*,LPD3DXBUFFER*,LPD3DXCONSTANTTABLE*);
HRESULT WINAPI Empty_D3DXCompileShaderFromResource (HMODULE,LPCTSTR,const D3DXMACRO*,LPD3DXINCLUDE,LPCSTR,LPCSTR,DWORD,LPD3DXBUFFER*,LPD3DXBUFFER*,LPD3DXCONSTANTTABLE*) { return 0; }
#endif

// D3DXCompileShaderFromFile
#ifdef UNICODE
HRESULT (WINAPI* _D3DXCompileShaderFromFile)(LPCWSTR, const D3DXMACRO*, LPD3DXINCLUDE, LPCSTR, LPCSTR, DWORD,
                                             LPD3DXBUFFER*, LPD3DXBUFFER*, LPD3DXCONSTANTTABLE*);

HRESULT WINAPI Empty_D3DXCompileShaderFromFile(LPCWSTR, const D3DXMACRO*, LPD3DXINCLUDE, LPCSTR, LPCSTR, DWORD,
                                               LPD3DXBUFFER*, LPD3DXBUFFER*, LPD3DXCONSTANTTABLE*) { return 0; }
#else
HRESULT (WINAPI* _D3DXCompileShaderFromFile) (LPCTSTR,const D3DXMACRO*,LPD3DXINCLUDE,LPCSTR,LPCSTR,DWORD,LPD3DXBUFFER*,LPD3DXBUFFER*,LPD3DXCONSTANTTABLE*);
HRESULT WINAPI Empty_D3DXCompileShaderFromFile (LPCTSTR,const D3DXMACRO*,LPD3DXINCLUDE,LPCSTR,LPCSTR,DWORD,LPD3DXBUFFER*,LPD3DXBUFFER*,LPD3DXCONSTANTTABLE*) { return 0; }
#endif

// D3DXCreateTextureShader
HRESULT (WINAPI* _D3DXCreateTextureShader)(const DWORD*, LPD3DXTEXTURESHADER*);
HRESULT WINAPI Empty_D3DXCreateTextureShader(const DWORD*, LPD3DXTEXTURESHADER*) { return 0; }

// D3DXCreateFont
#ifdef UNICODE
HRESULT (WINAPI* _D3DXCreateFont)(LPDIRECT3DDEVICE9, INT, UINT, UINT, UINT, BOOL, DWORD, DWORD, DWORD, DWORD, LPCWSTR,
                                  LPD3DXFONT*);

HRESULT WINAPI Empty_D3DXCreateFont(LPDIRECT3DDEVICE9, INT, UINT, UINT, UINT, BOOL, DWORD, DWORD, DWORD, DWORD, LPCWSTR,
                                    LPD3DXFONT*) { return 0; }
#else
HRESULT (WINAPI* _D3DXCreateFont) (LPDIRECT3DDEVICE9,INT,UINT,UINT,UINT,BOOL,DWORD,DWORD,DWORD,DWORD,LPCTSTR,LPD3DXFONT*);
HRESULT WINAPI Empty_D3DXCreateFont (LPDIRECT3DDEVICE9,INT,UINT,UINT,UINT,BOOL,DWORD,DWORD,DWORD,DWORD,LPCTSTR,LPD3DXFONT*) { return 0; }
#endif

static HINSTANCE hDx9Core_D3D9;
static HINSTANCE hDx9Core_D3DX9;
static BOOL nDx9CoreInit = FALSE;

/*
static void Dx9Core_Exit()
{
	FreeLibrary(hDx9Core_D3D9);
	FreeLibrary(hDx9Core_D3DX9);
}
*/

// Macro for easy handling of functions
#define _LOADFN(_rettype, _name, _empty, _params, _hinst, _str )		\
																		\
	_name = (_rettype (WINAPI *)_params) GetProcAddress(_hinst, _str);	\
																		\
	if(!_name) {														\
		_name = _empty;													\
		return 0;														\
	}

static INT32 Dx9Core_GetFunctions()
{
	if (!nDx9CoreInit) return 0;

	// D3D9.DLL
	_LOADFN(LPDIRECT3D9, _Direct3DCreate9, Empty_Direct3DCreate9, (UINT), hDx9Core_D3D9, "Direct3DCreate9");

	// D3DX9.DLL
	_LOADFN(HRESULT, _D3DXFillTextureTX, Empty_D3DXFillTextureTX, (LPDIRECT3DTEXTURE9, LPD3DXTEXTURESHADER),
	        hDx9Core_D3DX9, "D3DXFillTextureTX");

#ifdef UNICODE
	_LOADFN(HRESULT, _D3DXCreateEffectFromResource, Empty_D3DXCreateEffectFromResource,
	        (LPDIRECT3DDEVICE9, HMODULE, LPCWSTR, const D3DXMACRO*, LPD3DXINCLUDE, DWORD, LPD3DXEFFECTPOOL, LPD3DXEFFECT
		        *, LPD3DXBUFFER*), hDx9Core_D3DX9, "D3DXCreateEffectFromResourceW");
#else
	_LOADFN(HRESULT, _D3DXCreateEffectFromResource, Empty_D3DXCreateEffectFromResource, (LPDIRECT3DDEVICE9, HMODULE, LPCTSTR, const D3DXMACRO*,	LPD3DXINCLUDE, DWORD, LPD3DXEFFECTPOOL, LPD3DXEFFECT*, LPD3DXBUFFER*), hDx9Core_D3DX9, "D3DXCreateEffectFromResourceA");
#endif

#ifdef UNICODE
	_LOADFN(HRESULT, _D3DXCreateEffectFromFile, Empty_D3DXCreateEffectFromFile,
	        (LPDIRECT3DDEVICE9,LPCWSTR,const D3DXMACRO*,LPD3DXINCLUDE,DWORD,LPD3DXEFFECTPOOL,LPD3DXEFFECT*,LPD3DXBUFFER*
	        ), hDx9Core_D3DX9, "D3DXCreateEffectFromFileW");
#else
	_LOADFN(HRESULT, _D3DXCreateEffectFromFile, Empty_D3DXCreateEffectFromFile, (LPDIRECT3DDEVICE9,LPCTSTR,const D3DXMACRO*,LPD3DXINCLUDE,DWORD,LPD3DXEFFECTPOOL,LPD3DXEFFECT*,LPD3DXBUFFER*), hDx9Core_D3DX9, "D3DXCreateEffectFromFileA");
#endif

	_LOADFN(HRESULT, _D3DXCreateBuffer, Empty_D3DXCreateBuffer, (DWORD,LPD3DXBUFFER*), hDx9Core_D3DX9,
	        "D3DXCreateBuffer");

	_LOADFN(HRESULT, _D3DXLoadSurfaceFromMemory, Empty_D3DXLoadSurfaceFromMemory,
	        (LPDIRECT3DSURFACE9, const PALETTEENTRY*, const RECT*, LPCVOID,D3DFORMAT, UINT, const PALETTEENTRY*, const
		        RECT*, DWORD, D3DCOLOR), hDx9Core_D3DX9, "D3DXLoadSurfaceFromMemory");

#ifdef UNICODE
	_LOADFN(HRESULT, _D3DXCompileShaderFromResource, Empty_D3DXCompileShaderFromResource,
	        (HMODULE, LPCWSTR, const D3DXMACRO*, LPD3DXINCLUDE, LPCSTR, LPCSTR, DWORD, LPD3DXBUFFER*, LPD3DXBUFFER*,
		        LPD3DXCONSTANTTABLE*), hDx9Core_D3DX9, "D3DXCompileShaderFromResourceW");
#else
	_LOADFN(HRESULT, _D3DXCompileShaderFromResource, Empty_D3DXCompileShaderFromResource, (HMODULE, LPCTSTR, const D3DXMACRO*, LPD3DXINCLUDE, LPCSTR, LPCSTR, DWORD, LPD3DXBUFFER*, LPD3DXBUFFER*, LPD3DXCONSTANTTABLE*), hDx9Core_D3DX9, "D3DXCompileShaderFromResourceA");
#endif

#ifdef UNICODE
	_LOADFN(HRESULT, _D3DXCompileShaderFromFile, Empty_D3DXCompileShaderFromFile,
	        (LPCWSTR,const D3DXMACRO*,LPD3DXINCLUDE,LPCSTR,LPCSTR,DWORD,LPD3DXBUFFER*,LPD3DXBUFFER*,LPD3DXCONSTANTTABLE*
	        ), hDx9Core_D3DX9, "D3DXCompileShaderFromFileW");
#else
	_LOADFN(HRESULT, _D3DXCompileShaderFromFile, Empty_D3DXCompileShaderFromFile, (LPCTSTR,const D3DXMACRO*,LPD3DXINCLUDE,LPCSTR,LPCSTR,DWORD,LPD3DXBUFFER*,LPD3DXBUFFER*,LPD3DXCONSTANTTABLE*), hDx9Core_D3DX9, "D3DXCompileShaderFromFileA");
#endif

	_LOADFN(HRESULT, _D3DXCreateTextureShader, Empty_D3DXCreateTextureShader, (const DWORD*, LPD3DXTEXTURESHADER*),
	        hDx9Core_D3DX9, "D3DXCreateTextureShader");

#ifdef UNICODE
	_LOADFN(HRESULT, _D3DXCreateFont, Empty_D3DXCreateFont,
	        (LPDIRECT3DDEVICE9, INT, UINT, UINT, UINT, BOOL, DWORD, DWORD, DWORD, DWORD, LPCWSTR, LPD3DXFONT*),
	        hDx9Core_D3DX9, "D3DXCreateFontW");
#else
	_LOADFN(HRESULT, _D3DXCreateFont, Empty_D3DXCreateFont, (LPDIRECT3DDEVICE9, INT, UINT, UINT, UINT, BOOL, DWORD, DWORD, DWORD, DWORD, LPCTSTR, LPD3DXFONT*), hDx9Core_D3DX9, "D3DXCreateFontA");
#endif

	return 1;
}

INT32 Dx9Core_Init()
{
	hDx9Core_D3D9 = LoadLibrary(_T("d3d9.dll"));
	hDx9Core_D3DX9 = LoadLibrary(_T("D3DX9_43.dll"));

	if (!hDx9Core_D3D9 || !hDx9Core_D3DX9)
	{
		//MessageBox(NULL, _T("Loading of D3D9.DLL and D3DX9_43.DLL failed."), _T("Error"), MB_OK | MB_ICONERROR);
		nDx9CoreInit = FALSE;
		return 0;
	}

	nDx9CoreInit = TRUE;
	if (!Dx9Core_GetFunctions())
	{
		//MessageBox(NULL, _T("There was a problem while loading functions from D3D9.DLL and D3DX9_43.DLL"), _T("Error"), MB_OK | MB_ICONERROR);
		return 0;
	}

	return 1;
}
