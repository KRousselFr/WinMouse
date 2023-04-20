/**
 * \file main.c
 *
 * Programme de mouvement de la souris sous Windows NT.
 */

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
#define VERSION_PRG  L"0.5.0 du 20/04/2023"

/* taille maximale d'un message affiché par ce programme */
static const unsigned TAILLE_MAX_MSG = 1024U;

/* taille de la fenêtre */
static const int LARGEUR_FENETRE = 320 ;
static const int HAUTEUR_FENETRE = 200 ;

/* identifiant du "timer" de MAJ de la fenÃªtre de l'horloge */
static const UINT_PTR ID_EVNT_TIMER_SOURIS = 0x53554F4D ;   /* chaîne 'MOUS' en hexa */
/* délai entre deux mouvements de souris (en millisecondes) */
static const UINT DELAI_MVT_SOURIS = 1000U ;

/* amplitude maximale d'un mouvement de souris, en pixels */
static const LONG AMPLITUDE_MVT = 10L;

/* nombre maximal de mouvements hors fenêtre active avant recentrage */
static const unsigned MAX_MVMTS_DEHORS = 10U ;

/* == Messages affichés == */

static const WCHAR* CLASSE_FENETRE = L"WinMouse";
static const WCHAR* TITRE_PRG = L"WinMouse";
static const WCHAR* FMT_MSG_A_PROPOS =
		L"WinMouse version %ls.";


/*========================================================================*/
/*                          DEFINITIONS DE TYPES                          */
/*========================================================================*/



/*========================================================================*/
/*                           VARIABLES GLOBALES                           */
/*========================================================================*/

/* nombre de mouvements hors de la fenêtre active */
static unsigned int mvmts_dehors = 0 ;


/*========================================================================*/
/*                               FONCTIONS                                */
/*========================================================================*/

/* === MACROS === */

#define MOUSEEVENTF_MOVE_NOCOALESCE  0x2000


/* === FONCTIONS UTILITAIRES "PRIVEES" === */

/* génère des nombres pseudo-aléatoires plus ou moins uniformes */
static int
randint(int n)
{
	assert (n <= RAND_MAX) ;
	int end = RAND_MAX / n ;
	assert (end > 0) ;
	end *= n ;
	int r ;
	while ((r = rand ()) >= end) ;
	return r % n ;
}

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
	MessageBoxW (NULL, msgErr, TITRE_PRG, MB_ICONERROR) ;
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
		mvmts_dehors = 0;
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
}


/* === FONCTIONS "PUBLIQUES" === */

/* fonction appelée par le "timer" de la souris */
VOID CALLBACK
MouseTimerProc (HWND hwnd,
                UINT uMsg,
                UINT_PTR idEvent,
                DWORD dwTime)
{
	INPUT inpMsgs[1];
	ZeroMemory(inpMsgs, sizeof(inpMsgs));

	/* paramètres inutilisés */
	(void)hwnd; (void)uMsg; (void)dwTime;

	/* vérifie que l'on est bien le destinataire du message */
	if (idEvent != ID_EVNT_TIMER_SOURIS) return ;

	/* s'assure qu'on ne change pas de fenêtre */
	GarderDansFenetreCourante () ;

	/* détermine l'amplitude du mouvement de la souris à générer */
	inpMsgs[0].type = INPUT_MOUSE ;
	inpMsgs[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE ;
	inpMsgs[0].mi.dx = randint(AMPLITUDE_MVT) - (AMPLITUDE_MVT / 2);
	inpMsgs[0].mi.dy = randint(AMPLITUDE_MVT) - (AMPLITUDE_MVT / 2);

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
	HWND mainWnd = CreateWindowExW (WS_EX_APPWINDOW,
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
	if (mainWnd == NULL) {
		MsgErreurSys (L"Echec de CreateWindow() !") ;
		return -1 ;
	}

	/* affiche la fenêtre nouvellement créée */
	ShowWindow (mainWnd, nShowCmd) ;
	BOOL ok = UpdateWindow (mainWnd) ;
	if (!ok) {
		MsgErreurSys (L"Echec de UpdateWindow() !") ;
		return -1 ;
	}

	/* initialisation du générateur de nombres pseudo-aléatoires */
	srand((unsigned)rand());

	/* création du "timer" générant les évènements de mouvement de souris */
	UINT_PTR idTmr = SetTimer (mainWnd,
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
	KillTimer (mainWnd, ID_EVNT_TIMER_SOURIS) ;

	/* teste si une erreur est survenue */
	if (res == -1) {
		MsgErreurSys (L"Echec de GetMessage() !") ;
	}

	/* programme terminé */
	return (int) msg.wParam ;
}


