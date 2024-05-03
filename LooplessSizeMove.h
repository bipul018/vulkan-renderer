#ifndef LOOPLESSIZEMOVE_H
#define LOOPLESSIZEMOVE_H

/*
    LooplessSizeMove.h
    Defines functions for modal-less window resizing and movement in Windows

    Author: Nathaniel J Fries

    The author asserts no copyright, this work is released into the public domain.
*/

/*
    Performs actual move/size logic.
    Call before DispatchMessage
    Returns 1 if the passed message results in a move or resize, 0 otherwise
*/
BOOLEAN SizingCheck(const MSG *lpmsg);

/*
    Initiates logic for move/resize.
    Call instead of DefWindowProc
    Proxies to DefWindowProc for messages it does not need to handle.
    Arguments and returns same as DefWindowProc
*/
LRESULT CALLBACK LSMProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/*
    Perform thread-local cleanup of internal state.
    Necessary since the addition of aero features.
*/
void LSMCleanup();

#endif /* LOOPLESSIZEMOVE_H */
