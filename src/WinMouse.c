/**
 * \file main.c
 *
 * Programme de mouvement de la souris sous Windows NT.
 */

#define _WIN32_WINNT  0x0500
		/* programme conçu pour Windows 2000 ou ultérieur */

#include <assert.h>
#include <stdio.h>

#include <windows.h>

#include "resource.h"


/*========================================================================*/
/*                               CONSTANTES                               */
/*========================================================================*/

/**
 * \def VERSION_PRG
 *
 * Version courante du programme WinMouse.
 */
#define VERSION_PRG  L"0.7.7 du 15/05/2023"

/* taille maximale d'un message affiché par ce programme */
static const unsigned TAILLE_MAX_MSG = 1024U;

/* taille de la fenêtre */
static const int LARGEUR_FENETRE = 320 ;
static const int HAUTEUR_FENETRE = 200 ;

/* identifiant du "timer" de déplacement de la souris */
static const UINT_PTR ID_EVNT_TIMER_SOURIS = 0x53554F4D ;   /* chaîne 'MOUS' en hexa */
/* délai entre deux mouvements de souris (en millisecondes) */
static const UINT DELAI_MVT_SOURIS = 1000U ;

/* amplitude d'un mouvement de souris, en pixels */
#define AMPLITUDE_MVT  4L

/* liste cyclique des mouvement de souris (tracer un "cercle" miniature) */
static const POINT MVTS[] = {
		{  AMPLITUDE_MVT,              0 },
		{  AMPLITUDE_MVT,  AMPLITUDE_MVT },
		{              0,  AMPLITUDE_MVT },
		{ -AMPLITUDE_MVT,  AMPLITUDE_MVT },
		{ -AMPLITUDE_MVT,              0 },
		{ -AMPLITUDE_MVT, -AMPLITUDE_MVT },
		{              0, -AMPLITUDE_MVT },
		{  AMPLITUDE_MVT, -AMPLITUDE_MVT },
};

/* nombre maximal de mouvements hors fenêtre active avant recentrage */
static const unsigned int MAX_MVMTS_DEHORS = 10U ;

/* == Messages affichés == */

static const WCHAR* CLASSE_FENETRE = L"WinMouse";
static const WCHAR* TITRE_PRG = L"WinMouse";
static const WCHAR* FMT_MSG_A_PROPOS =
		L"WinMouse version %ls.";


/*========================================================================*/
/*                           VARIABLES GLOBALES                           */
/*========================================================================*/

/* nombre de mouvements hors de la fenêtre active */
static unsigned int mvmts_dehors = 0 ;

/* "handle" de la fenêtre principale de ce programme */
static HWND main_window = NULL ;

/* numéro d'index du mouvement courant */
static size_t idx_mvt = 0 ;

/* "flag" de traitement d'erreur en cours */
static volatile BOOL in_error = FALSE;


/*========================================================================*/
/*                               FONCTIONS                                */
/*========================================================================*/

/* === MACROS === */

#define MOUSEEVENTF_MOVE_NOCOALESCE  0x2000


/* === FONCTIONS UTILITAIRES "PRIVEES" === */

static DWORD
MsgErreurSys (const WCHAR* fmtMsg)
{
	DWORD codeErr = GetLastError () ;
	WCHAR msgErr[TAILLE_MAX_MSG] ;
	swprintf (msgErr, TAILLE_MAX_MSG, fmtMsg, codeErr) ;
	/* essaie d'obtenir une description de l'erreur en question */
	LPWSTR ptrMsgSys;
	DWORD res = FormatMessageW (FORMAT_MESSAGE_ALLOCATE_BUFFER
	                            | FORMAT_MESSAGE_FROM_SYSTEM,
	                            NULL,
	                            codeErr,
	                            MAKELANGID(LANG_SYSTEM_DEFAULT,
	                                       SUBLANG_SYS_DEFAULT),
	                            (LPWSTR)&ptrMsgSys,
	                            0,
	                            NULL) ;
	if (res != 0) {
		wcscat (msgErr, L"\n Message d'erreur système : ") ;
		wcsncat (msgErr, ptrMsgSys,
		         TAILLE_MAX_MSG - wcslen(msgErr)) ;
		LocalFree (ptrMsgSys) ;
	}
	/* ne pas multiplier les boîtes de message d'erreurs */
	if (!in_error) {
		in_error = TRUE ;
		MessageBoxW (main_window,
		             msgErr,
		             TITRE_PRG,
		             MB_ICONERROR | MB_SETFOREGROUND) ;
		in_error = FALSE ;
	}
	/* renvoie le code d'erreur système utilisé */
	return codeErr ;
}

static void
GarderDansFenetreCourante (void)
{
	/* retrouve la fenêtre active */
	HWND currWin = GetForegroundWindow () ;
	if (currWin == NULL) return ;

	/* détermine les coordonnées de cette fenêtre */
	RECT currWinRect ;
	BOOL ok = GetWindowRect (currWin, &currWinRect) ;
	if (!ok) {
		MsgErreurSys (L"Echec de GetWindowRect() !") ;
		return ;
	}

	/* position du curseur souris */
	POINT cursPos ;
	ok = GetCursorPos (&cursPos) ;
	if (!ok) {
		MsgErreurSys (L"Echec de GetCursorPos() !") ;
		return ;
	}
	/* si le pointeur est dans la fenêtre, rien à faire... */
	if (PtInRect (&currWinRect, cursPos)) {
		mvmts_dehors = 0 ;
		return ;
	}
	/* ... sinon, incrémente le compteur de mouvements hors de la fenêtre... */
	mvmts_dehors++;
	if (mvmts_dehors < MAX_MVMTS_DEHORS) return ;
	/* ... et si nécessaire ramène le curseur au milieu de la fenêtre voulue */

	/* retrouve la zone client de cette fenêtre */
	ok = GetClientRect (currWin, &currWinRect) ;
	if (!ok) {
		MsgErreurSys (L"Echec de GetWindowRect() !") ;
		return ;
	}

	/* milieu de la zone client de cette fenêtre */
	cursPos.x = currWinRect.right / 2 ;
	cursPos.y = currWinRect.bottom / 2 ;

	/* passe en coordonnées écran */
	ok = ClientToScreen (currWin, &cursPos) ;
	if (!ok) {
		MsgErreurSys (L"Echec de ClientToScreen() !") ;
		return ;
	}

	/* et enfin, déplace le curseur souris à cette position */
	ok = SetCursorPos (cursPos.x, cursPos.y) ;
	if (!ok) {
		MsgErreurSys (L"Echec de SetCursorPos() !") ;
		return ;
	}
	mvmts_dehors = 0 ;
}


/* === FONCTIONS "PUBLIQUES" === */

/* fonction appelée par le "timer" de la souris */
VOID CALLBACK
MouseTimerProc (HWND hwnd,
                UINT uMsg,
                UINT_PTR idEvent,
                DWORD dwTime)
{
	/* paramètres inutilisés */
	(void)hwnd; (void)uMsg; (void)dwTime;

	/* entrées utilisateur à générer */
	INPUT inpMsgs[1];
	ZeroMemory (inpMsgs, sizeof(inpMsgs)) ;

	/* vérifie que l'on est bien le destinataire du message */
	if (idEvent != ID_EVNT_TIMER_SOURIS) return ;

	/* s'assure qu'on ne change pas de fenêtre */
	GarderDansFenetreCourante () ;

	/* préparation de l'entrée pour le système */
	inpMsgs[0].type = INPUT_MOUSE ;
	inpMsgs[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE ;
	inpMsgs[0].mi.dx = MVTS[idx_mvt].x ;
	inpMsgs[0].mi.dy = MVTS[idx_mvt].y ;

	/* détermine le prochain mouvement de la souris à générer */
	idx_mvt = (idx_mvt + 1) % (sizeof(MVTS) / sizeof(POINT)) ;

	/* génère un mouvement de souris */
	UINT inpSent = SendInput (1U, inpMsgs, sizeof(INPUT)) ;
	if (inpSent < 1) {
		MsgErreurSys (L"Echec de SendInput() !") ;
		return ;
	}
}

/* fonction de gestion des messages de la fen�tre principale du programme */
LRESULT CALLBACK
MainWndProc (HWND hwnd,
             UINT message,
             WPARAM wParam,
             LPARAM lParam)
{
	static HFONT hFont ;
	static HPEN hPen ;
	PAINTSTRUCT ps ;
	HDC hDC ;
	RECT clientRect ;
	WCHAR txt[MAX_PATH] ;

	switch (message) {
	/* création de la fenêtre => objets de dessin utiles */
	case WM_CREATE:
		hFont = GetStockObject (SYSTEM_FONT) ;
		hPen = GetStockObject (BLACK_PEN) ;

		return 0;

	/* dessine le contenu de la fenêtre */
	case WM_PAINT:
		hDC = BeginPaint (hwnd, &ps) ;
		SelectObject (hDC, hFont) ;
		SelectObject (hDC, hPen) ;
		GetClientRect (hwnd, &clientRect) ;

		swprintf (txt, MAX_PATH, FMT_MSG_A_PROPOS, VERSION_PRG) ;
		DrawTextW (hDC,
		           txt,
		           -1,
		           &clientRect,
		           DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX) ;

		EndPaint (hwnd, &ps) ;

		return 0 ;

	/* ferme la fenêtre => quitte le programme */
	case WM_DESTROY:
		PostQuitMessage (0) ;
		return 0 ;
	}

	/* autres messages => traitement par défaut */
	return DefWindowProcW (hwnd, message, wParam, lParam) ;
}


/* === POINT D'ENTREE === */

int WINAPI
WinMain (HINSTANCE hInstance,
         HINSTANCE hPrevInstance,
         LPSTR lpCmdLine,
         int nShowCmd)
{
	/* paramètres inutiles */
	(void)hPrevInstance ;

	/* définition de la classe de notre fenêtre */
	WNDCLASSEXW wndClass ;
	wndClass.cbSize        = sizeof(WNDCLASSEXW) ;
	wndClass.style         = CS_HREDRAW | CS_VREDRAW ;
	wndClass.lpfnWndProc   = MainWndProc ;
	wndClass.cbClsExtra    = 0 ;
	wndClass.cbWndExtra    = 0 ;
	wndClass.hInstance     = hInstance ;
	wndClass.hIcon         = LoadIconW (hInstance,
	                                    MAKEINTRESOURCEW (ID_ICONE_WIN_MOUSE)) ;
	wndClass.hCursor       = LoadCursor (NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
	wndClass.lpszMenuName  = NULL ;
	wndClass.lpszClassName = CLASSE_FENETRE ;
	wndClass.hIconSm       = LoadIconW (hInstance,
	                                    MAKEINTRESOURCEW (ID_ICONE_WIN_MOUSE)) ;

	if (!RegisterClassExW (&wndClass)) {
		MsgErreurSys (L"Echec de RegisterClass() !") ;
		return -1 ;
	}

	/* créé la fenêtre principale (et unique) de notre application */
	main_window = CreateWindowExW (WS_EX_APPWINDOW,
	                               CLASSE_FENETRE,
	                               TITRE_PRG,
	                               WS_OVERLAPPEDWINDOW,
	                               CW_USEDEFAULT,
	                               CW_USEDEFAULT,
	                               LARGEUR_FENETRE,
	                               HAUTEUR_FENETRE,
	                               NULL,
	                               NULL,
	                               hInstance,
	                               NULL) ;
	if (main_window == NULL) {
		MsgErreurSys (L"Echec de CreateWindow() !") ;
		return -1 ;
	}

	/* affiche la fenêtre nouvellement créée */
	ShowWindow (main_window, nShowCmd) ;
	BOOL ok = UpdateWindow (main_window) ;
	if (!ok) {
		MsgErreurSys (L"Echec de UpdateWindow() !") ;
		return -1 ;
	}

	/* création du "timer" générant les évènements de mouvement de souris */
	UINT_PTR idTmr = SetTimer (main_window,
	                           ID_EVNT_TIMER_SOURIS,
	                           DELAI_MVT_SOURIS,
	                           MouseTimerProc) ;
	if (idTmr == 0U) {
		MsgErreurSys (L"Echec de SetTimer() !") ;
		return -1 ;
	}

	/* boucle de réception des messages destinés à notre programme */
	MSG msg;
	int res = GetMessageW (&msg, NULL, 0, 0) ;
	while (res > 0) {
		/* traite le message courant */
		TranslateMessage (&msg) ;
		DispatchMessageW (&msg) ;

		/* attend le message suivant */
		res = GetMessageW (&msg, NULL, 0, 0) ;
	}

	/* arrête et supprime le "timer" créé */
	KillTimer (main_window, ID_EVNT_TIMER_SOURIS) ;

	/* teste si une erreur est survenue */
	if (res == -1) {
		MsgErreurSys (L"Echec de GetMessage() !") ;
	}

	/* programme terminé */
	return (int) msg.wParam ;
}


