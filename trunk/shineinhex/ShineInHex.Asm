; Author : Ramunas Cvirka AKA ramguru
; Updated: 27/06/2010
; Credits: major bugs fixed by Wojciech Stryjewski

.686
.model flat,stdcall
option casemap:none

include ShineInHex.inc

.code

DllEntry PROC PUBLIC hIns:DWORD, reason:DWORD, reserved1:DWORD

    push   hIns
    pop    hInstance
    .if reason==DLL_PROCESS_ATTACH
		invoke CreateTextTable
		invoke CreateClass
		invoke GetProcessHeap
		mov    glob_heap, eax
    .elseif reason==DLL_PROCESS_DETACH
		invoke UnregisterClass, ADDR classname, hInstance
    .endif
    mov    eax, 1
    ret

DllEntry ENDP

DummyProc proc uses esi edi hWin:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
	
	; allocate two structures for saving unique properties
	mov    edi, sizeof HM            ; control properties
	add    edi, sizeof WndProcThunk  ; thunk code
	invoke HeapAlloc, glob_heap, HEAP_GENERATE_EXCEPTIONS or HEAP_ZERO_MEMORY, edi
	mov    ecx, hWin
	mov    HM.hWnd[eax], ecx
	mov    esi, eax
	add    esi, sizeof HM
		
	; figure out relative address
	mov    ecx, OFFSET ShineInHexProc
	sub    ecx, eax
	sub    ecx, edi
	mov	   DWORD PTR [esi+0], 042444C7h ; sorta trick
	mov	   DWORD PTR [esi+4], eax       ; sorta eax -> this
	mov	   BYTE  PTR [esi+8], 0E9h      ; sorta jmp
	mov	   DWORD PTR [esi+9], ecx       ; sorta relative proc addr	
	invoke SetWindowLong, hWin, GWL_WNDPROC, esi
	
	; return pProc(hWnd, uMsg, wParam, lParam);
	push   lParam
	push   wParam
	push   uMsg
	push   hWin
	call   esi
	
	ret
DummyProc endp

CreateTextTable	PROC uses esi edi

	mov	   edi, OFFSET hexlook
	xor	   ecx, ecx
	mov    esi, ' 00'
	.WHILE	cx <= 0FFh
		mov	   [edi], esi	; "00  "
		xor	   eax,   eax
		mov	   al, cl
		db	   0D4h,010h	;aam	16
		lea	   edx, [eax+7676h]
		shr	   edx, 4
		and	   edx, 909h
		lea	   eax, [eax+edx+2f2fh]
		xchg   al, ah
		mov	   [edi], ax	; Store hex
		add	   edi, 3		; Next
		inc	   cx
	.ENDW
	
	mov	DWORD PTR [hex_nl], 0A0Dh
	
	ret
CreateTextTable	ENDP

CreateClass proc
	LOCAL  wc:WNDCLASSEX

	mov	   wc.cbSize,      SIZEOF WNDCLASSEX
	mov    wc.style,       CS_GLOBALCLASS
	mov	   wc.lpfnWndProc, OFFSET DummyProc
	mov    wc.cbClsExtra,  0
	mov	   wc.cbWndExtra,  0
	push   hInstance
	pop	   wc.hInstance
	mov	   wc.hbrBackground, 0
	mov	   wc.lpszMenuName,  0
	mov	   wc.lpszClassName, OFFSET classname
	mov	   eax, 0
	mov	   wc.hIcon,   eax
	mov	   wc.hIconSm, eax
	invoke LoadCursor, 0, IDC_ARROW
	mov	   wc.hCursor, eax
	invoke RegisterClassEx, ADDR wc
	ret

CreateClass endp

AddRecentBytes proc X:DWORD
	
	mov    edi, X
	mov    eax, HM.pointer[ebx]
	sub	   edi, eax
	lea    esi, HM.modified_bytes[ebx]
	xor    ecx, ecx
	mov    edx, HM.modified_counter[ebx]
  @@:	
	test   edx, edx
	jz     @F
	sub    edx, 1
	mov    eax, DWORD PTR [esi+edx*4]
	cmp    eax, edi
	jz     @nop
	jmp    @B
  @@:
	cmp    HM.modified_counter[ebx], 24
	jnz    @F
	dec    HM.modified_counter[ebx]
  @@:	
	invoke RtlMoveMemory, ADDR HM.modified_bytes+4[ebx], ADDR HM.modified_bytes[ebx], 23*4 
	inc    HM.modified_counter[ebx]
	mov    eax, HM.modified_counter[ebx]
	dec    eax
	mov    HM.modified_bytes+0[ebx], edi
	
  @nop:	
	ret
AddRecentBytes endp

ShineInHexProc proc uses esi edi ebx hWin:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
	LOCAL tot_size:DWORD
	LOCAL f_old   :DWORD
	LOCAL dc      :DWORD
	LOCAL hbit	  :DWORD
	LOCAL col1a   :DWORD
	LOCAL col2a   :DWORD
	LOCAL col1b   :DWORD
	LOCAL col2b   :DWORD
	LOCAL update_start:DWORD
	LOCAL update_end  :DWORD
	LOCAL line_count  :DWORD
	LOCAL line_len    :DWORD
	LOCAL cx_wid      :DWORD
	LOCAL ps    :PAINTSTRUCT
	LOCAL sinfo :SCROLLINFO
	LOCAL rc    :RECT
	LOCAL set_rc:RECT
	LOCAL pt    :POINT
	LOCAL size_ :SIZEX
	LOCAL tem1  :DWORD
	LOCAL tem2  :DWORD
	LOCAL size1 :DWORD
	LOCAL size2 :DWORD
	LOCAL bound1:DWORD
	LOCAL bound2:DWORD
	LOCAL charB :BYTE
	LOCAL buf[32]:BYTE

	mov    ebx, hWin
	mov	   eax, uMsg
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.if eax==WM_CREATE
		invoke memfill, ADDR HM.modified_bytes[ebx], 24*4, 0
		mov    [HM.bk_color  + 0][ebx], 0ff8080h
		mov    [HM.bk_color  + 4][ebx], 0808080h
		mov    [HM.bk_color  + 8][ebx], 0000000h
		mov    [HM.bk_color  +12][ebx], 0000000h
		mov    [HM.bk_color  +16][ebx], 0ffffffh
		mov    [HM.bk_color  +20][ebx], 0ffffffh
		mov    [HM.bk_color  +24][ebx], 0ff8080h
		mov    [HM.bk_color  +28][ebx], 0808000h
		mov    [HM.bk_color  +32][ebx], 00000ffh
		mov    [HM.bk_color  +36][ebx], 0ffffffh
		mov    [HM.bk_color  +40][ebx], 0ffffffh
		mov    [HM.bk_color  +44][ebx], 0ffffffh
		mov    [HM.text_color+ 0][ebx], 0ffffffh
		mov    [HM.text_color+ 4][ebx], 0ffffffh
		mov    [HM.text_color+ 8][ebx], 0ff8080h
		mov    [HM.text_color+12][ebx], 0000000h
		mov    [HM.text_color+16][ebx], 0800000h
		mov    [HM.text_color+20][ebx], 0008000h
		mov    [HM.text_color+24][ebx], 0c0ffc0h
		mov    [HM.text_color+28][ebx], 0ffffffh
		mov    [HM.text_color+32][ebx], 0ffffffh
		mov    [HM.text_color+36][ebx], 0F0h
		mov    [HM.text_color+40][ebx], 0B0h
		mov    [HM.text_color+44][ebx], 070h
		mov    HM.undo_start[ebx], 0
		mov    HM.is_readonly[ebx], 0
		mov    HM.find_hwnd[ebx], 0
		mov    HM.select_hwnd[ebx], 0
		
		; Allocate smallest possible screen color buffer (1 * 8h + 8h = 10h)
		mov    HM.screen_colors_len[ebx], 1h
		invoke HeapAlloc, glob_heap, HEAP_GENERATE_EXCEPTIONS, 10h
		mov    HM.screen_colors_st2[ebx], eax
		invoke HeapAlloc, glob_heap, HEAP_GENERATE_EXCEPTIONS, 10h
		mov    HM.screen_colors_st3[ebx], eax
		
		invoke CreateFontIndirect, ADDR lf1
		mov    [HM.lf_choose+ 0][ebx], eax
		invoke CreateFontIndirect, ADDR lf2
		mov    [HM.lf_choose+ 4][ebx], eax
		invoke CreateFontIndirect, ADDR lf3
		mov    [HM.lf_choose+ 8][ebx], eax
		invoke CreateFontIndirect, ADDR lf4
		mov    [HM.lf_choose+12][ebx], eax
		invoke CreateFontIndirect, ADDR lf5
		mov    [HM.lf_choose+16][ebx], eax
			
		invoke CreatePopupMenu
		mov    HM.hex_popup[ebx], eax
		invoke CreatePopupMenu
		mov    HM.hex_popupA[ebx], eax
		
		mov    eax, offset hexlook
		mov    HM.pointer[ebx],   OFFSET hex_nl
		mov    HM._size[ebx],     1
		mov    HM.view_lenx[ebx], 8
		invoke AppendMenu,  HM.hex_popup[ebx], MF_STRING, 13004, ADDR menui_undo
		invoke AppendMenu,  HM.hex_popup[ebx], MF_STRING, 13005, ADDR menui_redo
		
		invoke AppendMenu,  HM.hex_popupA[ebx], MF_STRING, 13000, ADDR menui_copy_text
		invoke AppendMenu,  HM.hex_popupA[ebx], MF_STRING, 13001, ADDR menui_copy_hex
		invoke AppendMenu,  HM.hex_popupA[ebx], MF_STRING, 13006, ADDR menui_copy_db1
		invoke AppendMenu,  HM.hex_popupA[ebx], MF_STRING, 13007, ADDR menui_copy_db2
		invoke AppendMenu,  HM.hex_popupA[ebx], MF_STRING, 13008, ADDR menui_copy_x01
		invoke AppendMenu,  HM.hex_popupA[ebx], MF_STRING, 13009, ADDR menui_copy_x02
		
		invoke AppendMenu,  HM.hex_popup[ebx], MF_POPUP, HM.hex_popupA[ebx], ADDR menu_copy
		
		invoke AppendMenu,  HM.hex_popup[ebx], MF_SEPARATOR, 0, 0
		invoke AppendMenu,  HM.hex_popup[ebx], MF_STRING, 13002, ADDR menui_find
		invoke AppendMenu,  HM.hex_popup[ebx], MF_STRING, 13003, ADDR menui_select
		
		invoke SendMessage, HM.hWnd[ebx], HEXM_SETFONT, 0, 0

;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==HEXM_SETFONT
		mov    eax, wParam
		.if eax > 4
			xor    eax, eax
			ret
		.endif
		mov    eax, [HM.lf_choose+eax*4][ebx]
		mov    HM.font[ebx], eax
		invoke GetDC, HM.hWnd[ebx]
		mov    dc, eax
		invoke SelectObject, dc, HM.font[ebx]
		;............................
		invoke GetTextExtentPoint32, dc, ADDR measure_x, 52, ADDR size_
		mov    eax, size_.cx_
		mov    ecx, 26
		xor    edx, edx
		div    ecx
		add    eax, 1
		shr    eax, 1
		;............................
		mov    HM.fon_cx[ebx],      eax
		shl    eax, 3
		mov    HM.rc1.right[ebx],   eax
		add    eax, 8
		mov    HM.rc2.left[ebx],    eax
		mov    HM.sel_rc.left[ebx], eax
		mov    eax, size_.cy_
		mov    HM.fon_cy[ebx], eax
		add    eax, 1
		mov    HM.rc1.top[ebx], eax
		mov    HM.rc2.top[ebx], eax
		mov    HM.rc3.top[ebx], eax
		;------
		mov    eax, HM.fon_cx[ebx]
		lea    eax, [eax+eax*2]
		mul    HM.view_lenx[ebx]
		add    eax, HM.rc2.left[ebx]
		mov    HM.rc2.right[ebx], eax
		add    eax, 4
		mov    HM.rc3.left[ebx], eax
		;------
		mov    eax, HM.fon_cx[ebx]
		mul    HM.view_lenx[ebx]
		add    eax, HM.rc3.left[ebx]
		mov    HM.rc3.right[ebx], eax
		;------
		invoke GetClientRect, HM.hWnd[ebx], ADDR rc
		mov    eax, rc.bottom
		sub    eax, HM.rc1.top[ebx]
		xor    edx, edx
		div    HM.fon_cy[ebx]
		mov    HM.view_leny[ebx], eax
		;------
		mul    HM.fon_cy[ebx]
		add    eax, HM.rc1.top[ebx]
		mov    HM.rc1.bottom[ebx], eax
		mov    HM.rc2.bottom[ebx], eax
		mov    HM.rc3.bottom[ebx], eax
		invoke ReleaseDC, HM.hWnd[ebx], dc
		mov    sinfo.fMask, 0
		jmp    @resize
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax > HEXM_SETFONT && eax < HEXM_SETPOINTER ; all messages regarding grad,text,bk color go here 
		sub    eax, HEXM_SETOFFSETGRADCOL
		mov    ecx, wParam
		mov    edx, lParam
		mov    [HM.bk_color  +eax*4][ebx], ecx
		mov    [HM.text_color+eax*4][ebx], edx
		invoke SetScreenColors
		invoke DrawBase
		invoke InvalidateRect, HM.hWnd[ebx], 0, 0
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==HEXM_UNSETPOINTER
		mov    HM.pointer[ebx], OFFSET hex_nl
		mov    HM._size[ebx], 1
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==HEXM_SETPOINTER
		invoke ClearUndoBuf
			
		invoke HeapAlloc, glob_heap, HEAP_GENERATE_EXCEPTIONS, sizeof UNDO
		mov    HM.undo_start[ebx], eax
		mov    DWORD PTR [eax+ 0], 0
		mov    DWORD PTR [eax+ 4], 0
		mov    DWORD PTR [eax+ 8], 0
		mov    DWORD PTR [eax+12], 0
		mov    DWORD PTR [eax+16], 0		
		;----------------------------------
		xor    eax, eax
		mov    HM.sel_start[ebx],  eax
		mov    HM.sel_end[ebx],    eax
		mov    HM.vscrollpos[ebx], eax
		mov    HM.modified_counter[ebx], eax
		mov    HM.can_undo[ebx], eax
		mov    HM.can_redo[ebx], eax
		mov    sinfo.nPos,       eax
		mov    sinfo.fMask,      SIF_POS
		mov    eax, wParam
		mov    ecx, lParam
		mov    HM.pointer[ebx], eax
		mov    HM._size[ebx],   ecx
		invoke GetClientRect, HM.hWnd[ebx], ADDR rc
	@resize:
		mov    HM.view_lenx[ebx], 4
		mov    HM.view_leny[ebx], 1
			
		invoke InvalidateRect, HM.hWnd[ebx], 0, 0
			
		mov    ecx, rc.bottom
		shl    ecx, 16
		mov    eax, rc.right
		mov    cx,  ax
		mov    lParam, ecx
		jmp    @sizing
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==HEXM_SETOFFSET
		m2m    HM._start[ebx], wParam
		invoke InvalidateRect, HM.hWnd[ebx], 0, 0
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==HEXM_SETSEL
		mov    eax, HM._size[ebx]
		.if wParam > eax
			mov    wParam, eax
		.endif			
		.if lParam > eax
			mov    lParam, eax
		.endif				
		mov    eax, wParam
		sub    eax, HM._start[ebx]
		mov    HM.sel_start[ebx], eax
		xor    edx, edx
		div    HM.view_lenx[ebx]
		mov    ecx, HM._nMax[ebx]
		sub    ecx, eax
		mov    edx, HM._nMax[ebx]
		.if ecx < HM.view_leny[ebx] && edx > HM.view_leny[ebx]
			mov    ecx, HM._nMax[ebx]
			sub    ecx, HM.view_leny[ebx]
			inc    ecx
			mov    eax, ecx
		.endif
		.if edx > HM.view_leny[ebx]
			mov    HM.vscrollpos[ebx], eax
			invoke SetScrollPos, HM.hWnd[ebx], SB_VERT, eax, 1
		.endif	
		mov    eax, lParam
		sub    eax, HM._start[ebx]
		mov    HM.sel_end[ebx],   eax
		invoke SetScreenColors	
		invoke InvalidateRect, HM.hWnd[ebx], 0, 0
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==HEXM_UNDO
		invoke DoUndo
		invoke InvalidateRect, HM.hWnd[ebx], 0, 0
	.elseif eax==HEXM_REDO
		invoke DoRedo
		invoke InvalidateRect, HM.hWnd[ebx], 0, 0
	.elseif eax==HEXM_CANUNDO
		mov    eax, HM.can_undo[ebx]
		ret
	.elseif eax==HEXM_CANREDO
		mov    eax, HM.can_redo[ebx]
		ret
	.elseif eax==HEXM_SETREADONLY
		mov    eax, wParam
		mov    HM.is_readonly[ebx], eax
		ret
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==HEXM_INVALIDATE
		invoke SetScreenColors
		invoke InvalidateRect, HM.hWnd[ebx], 0, 0
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==HEXM_GETMENU
		mov    eax, HM.hex_popup[ebx]
		ret
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==WM_CHAR
		cmp    HM.is_readonly[ebx], 1
		jz     @char_forward
		cmp    HM.ctrl[ebx], 1
		jz     @sel_done
		mov    eax, wParam
		cmp    HM.input_mode[ebx], 1
		jz     @ASCII
		.if al > 2Fh && al < 3Ah || al > 40h && al < 47h || al > 60h && al < 67h
			push   eax
			.if HM.byte_nr[ebx] == 0
				invoke AllocUndo
			.endif
			pop    eax
			mov    edi, HM.pointer[ebx]
			mov    ecx, HM.sel_start[ebx]
			mov    edx, HM.sel_end[ebx]
			.if ecx > edx
				add    edi, edx
				xchg   ecx, edx
				dec    edx
			.elseif ecx < edx
				add    edi, ecx
				dec    edx
			.else
				add    edi, ecx
			.endif
			mov    charB, 0
			.if al > 2Fh && al < 3Ah
				sub    al, 30h
			.elseif al > 40h && al < 47h
				sub    al, 37h
			.elseif al > 60h && al < 67h
				sub    al, 57h
			.endif
		@next_char:
			push   eax
			mov    al, BYTE PTR [edi]
			mov    charB, al
			pop    eax
			.if HM.byte_nr[ebx] == 0
				and    charB, 00fh
				shl    al, 4
				.if edx==ecx
					inc    HM.byte_nr[ebx]
				.endif
			.elseif HM.byte_nr[ebx] == 1
				and    charB, 0f0h
				or     charB, al
				push   eax
				mov    al, charB
				mov    BYTE PTR [edi], al
				pop    eax
				pusha
				invoke AddRecentBytes, edi
				popa
				.if edx > ecx
					dec    edx
					inc    edi
					jmp    @next_char
				.endif
				invoke InvalidateRect, HM.hWnd[ebx], 0, 0
				mov    ecx, HM.sel_start[ebx]
				mov    edx, HM.sel_end[ebx]
				.if ecx == edx
					mov    wParam, VK_RIGHT
					invoke AllocUndo
					jmp    @right
				.else
					mov    HM.byte_nr[ebx], 0
					jmp    @selection
				.endif
			.endif
			or     charB, al
			push   eax
			mov    al, charB
			mov    BYTE PTR [edi], al
			pop    eax
			pusha
			invoke AddRecentBytes, edi
			popa
			.if edx > ecx
				dec    edx
				inc    edi
				shr    al, 4
				jmp    @next_char
			.endif
			invoke InvalidateRect, HM.hWnd[ebx], 0, 0
		.else
			; Forward unhandled WM_CHAR to parent window for custom shortcuts
			@char_forward:
			invoke GetParent, HM.hWnd[ebx]
			invoke SendMessage, eax, WM_CHAR, wParam, lParam
			jmp    @sel_done
		.endif
		jmp    @selection
		
	  @ASCII:
		push   eax
		.if HM.byte_nr[ebx] == 0
			invoke AllocUndo
		.endif
		pop    eax
		mov    edi, HM.pointer[ebx]
		mov    ecx, HM.sel_start[ebx]
		mov    edx, HM.sel_end[ebx]
		.if ecx > edx
			add    edi, edx
			xchg   ecx, edx
			dec    edx
		.elseif ecx < edx
			add    edi, ecx
			dec    edx
		.else
			add    edi, ecx
		.endif
	  @next_char1:
		pusha
		invoke AddRecentBytes, edi
		popa
		mov    BYTE PTR [edi], al				
		.if edx > ecx
			dec    edx
			inc    edi
			jmp    @next_char1
		.endif
		mov    ecx, HM.sel_start[ebx]
		mov    edx, HM.sel_end[ebx]
		.if ecx == edx
			mov    wParam, VK_RIGHT
			invoke AllocUndo
			jmp    @right
		.endif
		invoke InvalidateRect, HM.hWnd[ebx], 0, 0
	@selection:
		invoke AllocUndo
	@sel_done:
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==WM_KEYDOWN
		invoke GetKeyState, VK_SHIFT
		shr    eax, 15
		mov    HM.shift[ebx], eax
		invoke GetKeyState, VK_CONTROL
		shr    eax, 15
		mov    HM.ctrl[ebx], eax
		mov    eax, wParam
		.if eax > 20h && eax < 29h && HM.shift[ebx] == 0
			mov    ecx, HM.sel_start[ebx]
			mov    edx, HM.sel_end[ebx]
			.if ecx < edx || ecx > edx
				mov     HM.sel_start[ebx], edx
				mov     HM.sel_end[ebx],   edx
			.endif
		.elseif eax == VK_Z && HM.ctrl[ebx]
			invoke DoUndo
			invoke InvalidateRect, HM.hWnd[ebx], 0, 0
			xor    eax, eax
			ret
		.elseif eax == VK_Y && HM.ctrl[ebx]
			invoke DoRedo
			invoke InvalidateRect, HM.hWnd[ebx], 0, 0
			xor    eax, eax
			ret
		.elseif eax == VK_TAB
			invoke GetParent, HM.hWnd[ebx]
			mov    ecx, eax
			invoke GetNextDlgGroupItem, eax, HM.hWnd[ebx], HM.shift[ebx]
			invoke SetFocus, eax
			xor    eax, eax
			ret
		.endif
	@right:
		mov    eax, wParam
		.if eax == VK_RIGHT || eax == VK_LEFT
			mov    HM.byte_nr[ebx], 0
			.if eax == VK_RIGHT
				mov    eax, HM.view_leny[ebx]
				mov    ecx, HM.view_lenx[ebx]
				mov    edx, HM._size[ebx]
				;dec    edx
				cmp    HM.sel_end[ebx], edx
				jz     @skip
				;???
				mov    eax, HM.sel_end[ebx]
				xor    edx, edx
				div    HM.view_lenx[ebx]
				sub    eax, HM.vscrollpos[ebx]
				inc    eax
				cmp    eax, HM.view_leny[ebx]
				;???
				jnz    @no_new_line
				dec    eax
				add    eax, HM.vscrollpos[ebx]
				mul    HM.view_lenx[ebx]
				mov    ecx, HM.sel_end[ebx]
				sub    ecx, eax
				mov    eax, HM.view_lenx[ebx]
				dec    eax
				cmp    ecx, eax
				jnz    @no_new_line
				invoke SendMessage, HM.hWnd[ebx], WM_VSCROLL, SB_LINEDOWN, 0
			  @no_new_line:
				cmp    HM.shift[ebx], 1
				jz     @F
				inc    HM.sel_start[ebx]
				@@:	
				inc    HM.sel_end[ebx]
			.else
				mov    eax, HM.view_lenx[ebx]
				xor    ecx, ecx
				cmp    HM.sel_end[ebx], ecx
				jz     @skip
				;???
				mov    eax, HM.sel_end[ebx]
				xor    edx, edx
				div    HM.view_lenx[ebx]
				sub    eax, HM.vscrollpos[ebx]
				cmp    eax, 0
				;???
				jnz    @no_prev_line
				mov    eax, HM.vscrollpos[ebx]
				mul    HM.view_lenx[ebx]
				mov    ecx, HM.sel_end[ebx]
				sub    ecx, eax
				mov    eax, HM.view_lenx[ebx]
				cmp    ecx, 0
				jnz    @no_prev_line
				invoke SendMessage, HM.hWnd[ebx], WM_VSCROLL, SB_LINEUP, 0
			  @no_prev_line:
				cmp    HM.shift[ebx], 1
				jz     @F
				dec    HM.sel_start[ebx]
				@@:
				dec    HM.sel_end[ebx]
			.endif
		.elseif eax == VK_UP || eax == VK_DOWN
			.if eax == VK_UP
				mov    eax, HM.view_lenx[ebx]
				cmp    HM.sel_end[ebx], eax
				jb     @skip
				;???
				mov    eax, HM.sel_end[ebx]
				xor    edx, edx
				div    HM.view_lenx[ebx]
				sub    eax, HM.vscrollpos[ebx]
				cmp    eax, 0
				;???
				jg     @dec_y_pos
				invoke InvalidateRect, HM.hWnd[ebx], 0, 0
				invoke SendMessage, HM.hWnd[ebx], WM_VSCROLL, SB_LINEUP,   0
				jmp    @skip_a
			  @dec_y_pos:
				invoke InvalidateRect, HM.hWnd[ebx], ADDR HM.sel_rc[ebx], 0
			  @skip_a:
				mov    ecx, HM.view_lenx[ebx]
				cmp    HM.shift[ebx], 1
				jz     @F
				sub    HM.sel_start[ebx], ecx
				@@:
				sub    HM.sel_end[ebx],   ecx
				invoke SetScreenColors	
			.else
				mov    ecx, HM.view_leny[ebx]
				mov    eax, HM._size[ebx]
				sub    eax, HM.view_lenx[ebx]
				cmp    HM.sel_end[ebx], eax
				jge    @skip
				;???
				mov    eax, HM.sel_end[ebx]
				xor    edx, edx
				div    HM.view_lenx[ebx]
				sub    eax, HM.vscrollpos[ebx]
				inc    eax
				cmp    eax, ecx
				;???
				jl     @inc_y_pos
				invoke InvalidateRect, HM.hWnd[ebx], 0, 0
				invoke SendMessage, HM.hWnd[ebx], WM_VSCROLL, SB_LINEDOWN, 0
				jmp    @skip_b
			  @inc_y_pos:
				invoke InvalidateRect, HM.hWnd[ebx], ADDR HM.sel_rc[ebx], 0
			  @skip_b:	
				mov    ecx, HM.view_lenx[ebx]
				cmp    HM.shift[ebx], 1
				jz     @F
				add    HM.sel_start[ebx], ecx
				@@:
				add    HM.sel_end[ebx],   ecx
				invoke SetScreenColors	
			.endif
			jmp    @skip
		.elseif eax == VK_PGDN
			mov    eax, HM.view_lenx[ebx]
			mov    edx, HM.sel_end[ebx]
			add    edx, eax
			cmp    edx, HM._size[ebx]
			jae    @pgdn
			mov    ecx, HM.view_leny[ebx]
			mul    ecx
			mov    edx, HM.sel_end[ebx]
			add    edx, eax
			cmp    edx, HM._size[ebx]
			jae    @F
			cmp    HM.shift[ebx], 1
			jz     @no_shift0
			add    HM.sel_start[ebx],  eax
		  @no_shift0:
			add    HM.sel_end[ebx],    eax
			invoke SendMessage, HM.hWnd[ebx], WM_VSCROLL, SB_PAGEDOWN, 0
			jmp    @pgdn
		  @@:
			mov    eax, HM._nMax[ebx]
			sub    eax, HM.view_leny[ebx]
			inc    eax
			mov    HM.vscrollpos[ebx], eax
			mov    eax, HM.sel_end[ebx]
			xor    edx, edx 
			div    HM.view_lenx[ebx]
			mov    ecx, HM._nMax[ebx]
			sub    ecx, eax
			mov    eax, ecx
			mul    HM.view_lenx[ebx]
			mov    ecx, HM._size[ebx]
			mov    edx, HM.sel_end[ebx]
			add    edx, eax
			cmp    edx, ecx
			jb     @F
			dec    ecx
			cmp    HM.shift[ebx], 1
			jz     @no_shift1
			mov    HM.sel_start[ebx],  ecx
		  @no_shift1:	
			mov    HM.sel_end[ebx],    ecx
			invoke SetScrollPos, HM.hWnd[ebx], SB_VERT, HM._nMax[ebx], 1
			jmp    @pgdn
		  @@:
			cmp    HM.shift[ebx], 1
			jz     @no_shift2
			add    HM.sel_start[ebx],  eax
		  @no_shift2:
			add    HM.sel_end[ebx],    eax
			invoke SetScrollPos, HM.hWnd[ebx], SB_VERT, HM._nMax[ebx], 1
		  @pgdn:	
		.elseif eax == VK_PGUP
			mov    eax, HM.view_lenx[ebx]
			mov    edx, HM.sel_end[ebx]
			sub    edx, eax
			cmp    edx, 0
			jl     @pgup
			mov    ecx, HM.view_leny[ebx]
			mul    ecx
			mov    edx, HM.sel_end[ebx]
			sub    edx, eax
			cmp    edx, 0
			jl     @F
			cmp    HM.shift[ebx], 1
			jz     @no_shift3
			sub    HM.sel_start[ebx],  eax
		  @no_shift3:			
			sub    HM.sel_end[ebx],    eax
			invoke SendMessage, HM.hWnd[ebx], WM_VSCROLL, SB_PAGEUP, 0
			jmp    @pgup
		  @@:
			mov    eax, HM.sel_end[ebx]
			xor    edx, edx 
			div    HM.view_lenx[ebx]
			mov    HM.vscrollpos[ebx], 0
			mul    HM.view_lenx[ebx]
			cmp    HM.shift[ebx], 1
			jz     @no_shift4
			sub    HM.sel_start[ebx],  eax
		  @no_shift4:
			sub    HM.sel_end[ebx],    eax
			invoke SetScrollPos, HM.hWnd[ebx], SB_VERT, 0, 1 
		  @pgup:
		.elseif eax==VK_HOME || eax==VK_END
			.if eax==VK_HOME
				mov    ecx, HM.view_lenx[ebx]
				mov    eax, HM.sel_end[ebx]
				xor    edx, edx
				div    ecx
				cmp    edx, 0
				jz     @skip
				sub    HM.sel_start[ebx], edx
				sub    HM.sel_end[ebx],   edx
				mov    eax, 1
			.else
				mov    ecx, HM.view_lenx[ebx]
				mov    eax, HM.sel_end[ebx]
				inc    eax
				xor    edx, edx
				div    ecx
				cmp    edx, 0
				jz     @skip
				mov    eax, HM.view_lenx[ebx]
				sub    eax, edx
				add    HM.sel_start[ebx], eax
				add    HM.sel_end[ebx],   eax
				mov    eax, HM.view_lenx[ebx]
			.endif
		.else
			; Forward unrecognized WM_KEYDOWN to parent window for custom shortcuts
			invoke GetParent, HM.hWnd[ebx]
			invoke SendMessage, eax, WM_KEYDOWN, wParam, lParam
			jmp    @key_done
		.endif
		invoke SetScreenColors	
		invoke InvalidateRect, HM.hWnd[ebx], ADDR HM.sel_rc[ebx], 0
	@skip:
		mov    rc.left, 0
		mov    rc.top, 0
		mov    eax, HM.fon_cy[ebx]
		mov    rc.bottom, eax
		mov    eax, HM.fon_cx[ebx]
		shl    eax, 3
		mov    rc.right, eax
		invoke InvalidateRect, HM.hWnd[ebx], ADDR rc, 0
	@key_done:
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==WM_SETFOCUS
		invoke SetFocus,    HM.hWnd[ebx]
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==WM_GETDLGCODE
		mov    eax, DLGC_WANTALLKEYS+DLGC_WANTTAB
		ret
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==WM_MOUSEACTIVATE
		invoke SetFocus, HM.hWnd[ebx]
		mov    eax, MA_ACTIVATE
		ret
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==WM_VSCROLL
		mov    sinfo.cbSize,  sizeof SCROLLINFO
		mov    sinfo.fMask,   SIF_ALL
		invoke GetScrollInfo, HM.hWnd[ebx], SB_VERT, ADDR sinfo
		push   sinfo.nPos
		mov    eax, wParam
		and    eax, 0ffffh
		cmp    eax, SB_TOP
		je     top
		cmp    eax, SB_BOTTOM
		je     bottom
		cmp    eax, SB_LINEUP
		je     lineup
		cmp    eax, SB_LINEDOWN
		je     linedown
		cmp    eax, SB_PAGEUP
		je     pageup
		cmp    eax, SB_PAGEDOWN
		je     pagedown
		cmp    eax, SB_THUMBTRACK
		je     ttrack
		cmp    eax, SB_THUMBPOSITION
		je     ttrack
		mov    eax, 0
		ret
top:
		mov    eax, sinfo.nMin
		mov    sinfo.nPos, eax
		jmp    @F
bottom:
		mov    eax, sinfo.nMax
		mov    sinfo.nPos, eax
		jmp    @F
lineup:
		dec    sinfo.nPos
		jmp    @F
linedown:
		inc    sinfo.nPos
		jmp    @F
pageup:
		mov    eax, sinfo.nPage
		sub    sinfo.nPos, eax
		jmp    @F
pagedown:
		mov    eax, sinfo.nPage
		add    sinfo.nPos, eax
		mov    ecx, sinfo.nMax
		sub    ecx, sinfo.nPage
		inc    ecx
		cmp    sinfo.nPos, ecx
		jb     @F
		mov    sinfo.nPos, ecx
		jmp    @F
ttrack:
		mov    eax, sinfo.nTrackPos
		mov    sinfo.nPos, eax
@@:
		.if SDWORD PTR sinfo.nPos < 0
			mov sinfo.nPos, 0
		.endif
		mov    ecx, sinfo.nPos
		pop    eax
		sub    eax, ecx
		add    ecx, sinfo.nPage
		dec    ecx
		.if eax != 0 && ecx <= sinfo.nMax
			push   eax
			mov    eax, sinfo.nPos
			mov    HM.vscrollpos[ebx], eax
			invoke SetScrollInfo, HM.hWnd[ebx], SB_VERT, ADDR sinfo, TRUE
			invoke MemCopy, ADDR HM.rc3[ebx], ADDR rc, sizeof RECT
			mov    rc.left, 0
			pop    eax
			mul    HM.fon_cy[ebx]
			mov    ecx, eax
			invoke ScrollWindowEx, HM.hWnd[ebx], 0, ecx, ADDR rc, ADDR rc, 0, 0, SW_INVALIDATE
			invoke SetScreenColors	
		.endif
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==WM_MOUSEWHEEL
		mov    eax, wParam
		shr    eax, 16
		xor    ecx, ecx
		cmp    ax, 0
		jge    @F
		inc    ecx
	  @@:
		invoke SendMessage, HM.hWnd[ebx], WM_VSCROLL, ecx, 0
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==WM_LBUTTONDOWN || eax==WM_LBUTTONUP || eax==WM_MOUSEMOVE && wParam == MK_LBUTTON
		.if eax==WM_LBUTTONDOWN
			mov    hex_set2, 0
			invoke SetCapture, HM.hWnd[ebx]
			mov    ecx, lParam
			shr    ecx, 16
			cmp    ecx, HM.rc1.top[ebx]
			ja     @F
			ret
			@@:
		.elseif eax==WM_LBUTTONUP
			invoke ReleaseCapture
		.endif
		mov    eax, lParam
		and    eax, 0ffffh
		.if eax >= HM.rc2.left[ebx] && hex_set2 == 0
			.if eax >= HM.rc3.left[ebx]
				mov    HM.input_mode[ebx], 1
				sub    eax, HM.rc3.left[ebx]
				mov    ecx, HM.fon_cx[ebx]
				xor    edx, edx
				div    ecx
			.else
				mov    HM.input_mode[ebx], 0
				sub    eax, HM.rc2.left[ebx]
				mov    ecx, HM.fon_cx[ebx]
				lea    ecx, [ecx+ecx*2]
				xor    edx, edx
				div    ecx
				mov    ecx, HM.fon_cx[ebx]
				shl    ecx, 1
			.endif
			.if edx > ecx
				inc eax
			.endif
			mov    edi, eax
			mov    eax, lParam
			shr    eax, 16
			sub    eax, HM.rc1.top[ebx]
			mov    ecx, HM.fon_cy[ebx]
			xor    edx, edx
			div    ecx
			mov    ecx, HM.view_lenx[ebx]
			mul    ecx
			add    edi, eax
			mov    eax, HM.vscrollpos[ebx]
			mul    HM.view_lenx[ebx]
			add    edi, eax
			.if uMsg==WM_LBUTTONDOWN
				mov    ecx, HM._size[ebx]
				cmp    edi, ecx
				jbe    @F
				mov    edi, ecx
				@@:
				mov    eax, HM.sel_start[ebx]
				mov    HM.sel_last[ebx], eax
				invoke InvalidateSel, HM.hWnd[ebx]
				mov    hex_set1, 1
				mov    hex_set2, 0
				mov    HM.sel_start[ebx], edi
				mov    HM.sel_end[ebx],   edi
				invoke SetScreenColors
				mov    eax, HM.sel_end[ebx]
				mov    HM.sel_last[ebx], eax
				mov    HM.byte_nr[ebx], 0
				invoke InvalidateSel, HM.hWnd[ebx] ; ??? maybe remove	
			.elseif uMsg==WM_LBUTTONUP
				mov    hex_set2, 1
				cmp    HM.timer_direction[ebx], 0
				jnz    @F
				mov    HM.sel_end[ebx], edi
			  @@:	
				invoke KillTimer, HM.hWnd[ebx], 100
				mov    ecx, HM._size[ebx]
				cmp    HM.sel_end[ebx], ecx
				jb     @no_size_over
				mov    HM.sel_end[ebx], ecx
			  @no_size_over:
				invoke SetScreenColors
				mov    HM.timer_direction[ebx], 0
				mov    eax, HM.sel_end[ebx]
				mov    HM.sel_last[ebx], eax
				invoke InvalidateSel, HM.hWnd[ebx]
			.elseif uMsg==WM_MOUSEMOVE
				mov    eax, HM.sel_end[ebx]
				mov    ecx, lParam
				shr    ecx, 16
				.if ecx < HM.rc2.top[ebx] || ecx > 0F000h 
					cmp    HM.timer_direction[ebx], 0
					jnz    @F
					invoke SetTimer, HM.hWnd[ebx], 100, 20, 0
					mov    HM.timer_direction[ebx], 1
					@@:
				.elseif ecx > HM.rc1.bottom[ebx]
					cmp    HM.timer_direction[ebx], 0
					jnz    @F
					invoke SetTimer, HM.hWnd[ebx], 100, 20, 0
					mov    HM.timer_direction[ebx], 2
					@@:
				.elseif eax != edi
					mov    ecx, HM._size[ebx]
					cmp    edi, ecx
					jbe    @F
					mov    edi, ecx
					@@:
					mov    HM.sel_last[ebx], eax
					invoke KillTimer, HM.hWnd[ebx], 100
					mov    HM.timer_direction[ebx], 0
					mov    HM.sel_end[ebx], edi
					invoke SetScreenColors
					invoke InvalidateSel, HM.hWnd[ebx]
				.endif
			.endif			
		.endif
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==WM_TIMER
		.if HM.timer_direction[ebx] == 1
			mov    eax, HM.sel_end[ebx]
			mov    ecx, HM.view_lenx[ebx]
			sub    eax, ecx
			cmp    eax, 0
			jl     @F
			sub    HM.sel_end[ebx], ecx
			mov    eax, HM.rc1.top[ebx]
			add    eax, HM.fon_cy[ebx]
			mov    rc.top, eax
			add    eax, HM.fon_cy[ebx]
			mov    rc.bottom, eax
			mov    eax, HM.rc2.left[ebx]
			mov    rc.left, eax
			mov    eax, HM.rc3.right[ebx]
			mov    rc.right, eax
			invoke InvalidateRect, HM.hWnd[ebx], ADDR rc, 0
			invoke SendMessage, HM.hWnd[ebx], WM_VSCROLL, SB_LINEUP, 0
		  @@:
		.elseif HM.timer_direction[ebx] == 2
			mov    eax, HM.sel_end[ebx]
			mov    ecx, HM.view_lenx[ebx]
			add    eax, ecx
			cmp    eax, HM._size[ebx]
			jge    @F
			add    HM.sel_end[ebx],   ecx
			mov    eax, HM.rc1.bottom[ebx]
			sub    eax, HM.fon_cy[ebx]
			mov    rc.bottom, eax
			sub    eax, HM.fon_cy[ebx]
			mov    rc.top, eax
			mov    eax, HM.rc2.left[ebx]
			mov    rc.left, eax
			mov    eax, HM.rc3.right[ebx]
			mov    rc.right, eax
			invoke InvalidateRect, HM.hWnd[ebx], ADDR rc, 0
			invoke SendMessage, HM.hWnd[ebx], WM_VSCROLL, SB_LINEDOWN, 0
		  @@:
		.endif
		mov    rc.left, 0
		mov    rc.top, 0
		mov    eax, HM.fon_cy[ebx]
		mov    rc.bottom, eax
		mov    eax, HM.fon_cx[ebx]
		shl    eax, 3
		mov    rc.right, eax
		invoke InvalidateRect, HM.hWnd[ebx], ADDR rc, 0
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==WM_RBUTTONUP
		mov    ecx, HM.sel_start[ebx]
		sub    ecx, HM.sel_end[ebx]
		.if ecx==0
			mov edi, MF_GRAYED
		.else
			mov edi, MF_ENABLED
		.endif
		invoke EnableMenuItem, HM.hex_popupA[ebx], 13000, edi
		invoke EnableMenuItem, HM.hex_popupA[ebx], 13001, edi
		invoke EnableMenuItem, HM.hex_popupA[ebx], 13006, edi
		invoke EnableMenuItem, HM.hex_popupA[ebx], 13008, edi
		mov    edi, HM.can_undo[ebx]
		xor    edi, 1
		invoke EnableMenuItem, HM.hex_popup[ebx], 13004, edi
		mov    edi, HM.can_redo[ebx]
		xor    edi, 1
		invoke EnableMenuItem, HM.hex_popup[ebx], 13005, edi

		; Send HEXN_SHOWMENU notify so parent can customize menu
		mov    HM.nm.hdr.code[ebx], HEXN_SHOWMENU
		m2m    HM.nm.menu.hex_popup[ebx], HM.hex_popup[ebx]
		invoke SendNotify

		invoke GetCursorPos, ADDR pt
		invoke TrackPopupMenuEx, HM.hex_popup[ebx], TPM_LEFTALIGN, pt.x, pt.y, HM.hWnd[ebx], 0
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==WM_COMMAND
		mov    eax, wParam
		.if eax==13000 || eax==13001
			invoke OpenClipboard, HM.hWnd[ebx]
			invoke EmptyClipboard		
			mov    edx, HM.sel_start[ebx]
			mov    ecx, HM.sel_end[ebx]
			.if edx > ecx
				sub    edx, ecx
				mov    ecx, edx
			.else
				sub    ecx, edx
			.endif
			shl    ecx, 1
			inc    ecx
			invoke GlobalAlloc, GMEM_MOVEABLE, ecx
			mov    hex_clip_copy, eax
			invoke GlobalLock, hex_clip_copy
			mov    hex_clip_point, eax
			test   eax, eax
			jz     @exit
			mov    esi, HM.pointer[ebx]
			mov    edi, hex_clip_point
			mov    ecx, HM.sel_start[ebx]
			mov    edx, HM.sel_end[ebx]
			.if ecx > edx
				add    esi, edx
				sub    ecx, edx
			.else
				add    esi, ecx
				sub    edx, ecx
				mov    ecx, edx
			.endif
			.if wParam==13000
			@@:	
				cmp    ecx, 0
				jz     @F
				mov    al, BYTE PTR [esi]
				mov    BYTE PTR [edi], al
				inc    esi
				inc    edi
				dec    ecx
				jmp    @B
			@@:
			.else
				push   ebx
				mov    ebx, offset hexlook
			@@:	
				xor    eax, eax
				cmp    ecx, 0
				jz     @F
				mov    al, BYTE PTR [esi]
				lea    eax, [eax+eax*2]
				mov    dl, BYTE PTR [eax+ebx]
				mov    BYTE PTR [edi], dl
				inc    edi
				mov    dl, BYTE PTR [eax+ebx+1]
				mov    BYTE PTR [edi], dl
				inc    esi
				inc    edi
				dec    ecx
				jmp    @B
			@@:
				pop    ebx
			.endif
			mov    BYTE PTR [edi], 0
			invoke GlobalUnlock, hex_clip_copy
			invoke SetClipboardData, CF_TEXT, hex_clip_copy
			invoke CloseClipboard
		@exit:
			invoke GlobalFree, hex_clip_copy
		.elseif eax == 13002
			.if HM.find_hwnd[ebx]
				invoke ShowWindow, HM.find_hwnd[ebx], SW_SHOW
				invoke SetActiveWindow, HM.find_hwnd[ebx]
			.else
				invoke CreateDialogParamW, hInstance, 666, HM.hWnd[ebx], ADDR DlgFind, ebx
			.endif
		.elseif eax == 13003
			.if HM.select_hwnd[ebx]
				invoke ShowWindow, HM.select_hwnd[ebx], SW_SHOW
				invoke SetActiveWindow, HM.select_hwnd[ebx]
			.else
				invoke CreateDialogParamW, hInstance, 999, HM.hWnd[ebx], ADDR DlgSelect, ebx
			.endif
		.elseif eax == 13004
			invoke DoUndo
			invoke InvalidateRect, HM.hWnd[ebx], 0, 0
		.elseif eax == 13005
			invoke DoRedo
			invoke InvalidateRect, HM.hWnd[ebx], 0, 0
		.elseif eax == 13006 || eax == 13008;   db selection; \x00 selection
			.if eax == 13006
				m2m    tem1, OFFSET m3
				m2m    tem2, OFFSET m4
				mov    size1, 8
				mov    size2, 5
			.else
				m2m    tem1, OFFSET m5
				m2m    tem2, OFFSET m6
				mov    size1, 5
				mov    size2, 4
			.endif
			invoke OpenClipboard, HM.hWnd[ebx]
			invoke EmptyClipboard		
			mov    edx, HM.sel_start[ebx]
			mov    ecx, HM.sel_end[ebx]
			.if edx > ecx
				sub    edx, ecx
				mov    ecx, edx
			.else
				sub    ecx, edx
			.endif
			; 5*bytes+(bytes/4)+4
			mov    edx, ecx
			lea    edx, [edx+edx*4]
			shr    ecx, 2
			add    ecx, edx
			add    ecx, 5
				
			invoke GlobalAlloc, GMEM_MOVEABLE, ecx
			mov    hex_clip_copy, eax
			invoke GlobalLock, hex_clip_copy
			mov    hex_clip_point, eax
			test   eax, eax
			jz     @exit_
			mov    esi, HM.pointer[ebx]
			mov    edi, hex_clip_point ; start of clipboard data (pointer)
			mov    ecx, HM.sel_start[ebx]
			mov    edx, HM.sel_end[ebx]
			.if edx > ecx
				add    esi, ecx ; pointer to selection start
				sub    edx, ecx
				mov    ecx, edx ; ecx - number of selected bytes
			.else
				add    esi, edx
				sub    ecx, edx
			.endif
			
			mov    BYTE PTR [edi], 0
			xor    edx, edx
		@@:
			test   ecx, ecx
			jz     @exitA
			mov    eax, edx
			shr    eax, 4
			shl    eax, 4
			.if eax == edx
				.if edx > 1 && wParam == 13006
					mov   BYTE PTR [edi-1], 13
					mov   BYTE PTR [edi+0], 10
					mov   BYTE PTR [edi+1], 0
					inc   edi
				.elseif edx > 1
					mov   BYTE PTR [edi+0], 34
					mov   BYTE PTR [edi+1], 13
					mov   BYTE PTR [edi+2], 10
					mov   BYTE PTR [edi+3], 0
					add   edi, 3
				.endif
				push   ecx
				push   edx
				xor    ecx, ecx
				mov    cl, BYTE PTR [esi]
				invoke wsprintf, ADDR buf, tem1, ecx
				invoke szCatStr, edi, ADDR buf
				pop    edx
				pop    ecx
				add    edi, size1
			.else
				push   ecx
				push   edx
				xor    ecx, ecx
				mov    cl, BYTE PTR [esi]
				invoke wsprintf, ADDR buf, tem2, ecx
				invoke szCatStr, edi, ADDR buf
				pop    edx
				pop    ecx
				add    edi, size2
			.endif
			dec    ecx
			inc    edx
			inc    esi
			jmp    @B
		@exitA:
			.if wParam == 13008
				mov    BYTE PTR [edi+0], 34 ; "
				mov    BYTE PTR [edi+1], 0
			.else
				mov    BYTE PTR [edi-1], 0
			.endif
			invoke GlobalUnlock, hex_clip_copy
			invoke SetClipboardData, CF_TEXT, hex_clip_copy
			invoke CloseClipboard
		@exit_:
			invoke GlobalFree, hex_clip_copy
			
		.elseif eax == 13007 || eax == 13009; db not selection; \x00 not selection
			.if eax == 13007
				m2m    tem1, OFFSET m3
				m2m    tem2, OFFSET m4
				mov    size1, 8
				mov    size2, 5
			.else
				m2m    tem1, OFFSET m5
				m2m    tem2, OFFSET m6
				mov    size1, 5
				mov    size2, 4
			.endif
			invoke OpenClipboard, HM.hWnd[ebx]
			invoke EmptyClipboard		
			mov    edx, HM.sel_start[ebx]
			mov    ecx, HM.sel_end[ebx]
			.if edx > ecx
				xchg   edx, ecx
			.endif
			sub    ecx, edx
			mov    bound1, edx
			mov    bound2, ecx
			mov    eax, HM._size[ebx]
			sub    eax, ecx
			mov    ecx, eax
			; 5*bytes+(bytes/4)+4 : exactly for "db" & less than needed for "\x"
			mov    edx, ecx
			lea    edx, [edx+edx*4]
			shr    ecx, 2
			add    ecx, edx
			add    ecx, 5
				
			invoke GlobalAlloc, GMEM_MOVEABLE, ecx
			mov    hex_clip_copy, eax
			invoke GlobalLock, hex_clip_copy
			mov    hex_clip_point, eax
			test   eax, eax
			jz     @exit__
			mov    esi, HM.pointer[ebx]
			mov    edi, hex_clip_point ; start of clipboard data (pointer)
			mov    ecx, HM._size[ebx]
			sub    ecx, bound2
			
			mov    BYTE PTR [edi], 0
			xor    edx, edx
		@@:
			test   ecx, ecx
			jz     @exitB
			.if edx == bound1
				add    esi, bound2
			.endif
			mov    eax, edx
			shr    eax, 4
			shl    eax, 4
			.if eax == edx
				.if edx > 1 && wParam == 13007
					mov   BYTE PTR [edi-1], 13
					mov   BYTE PTR [edi+0], 10
					mov   BYTE PTR [edi+1], 0
					inc   edi
				.elseif edx > 1
					mov   BYTE PTR [edi+0], 34
					mov   BYTE PTR [edi+1], 13
					mov   BYTE PTR [edi+2], 10
					mov   BYTE PTR [edi+3], 0
					add   edi, 3
				.endif
				push   ecx
				push   edx
				xor    ecx, ecx
				mov    cl, BYTE PTR [esi]
				invoke wsprintf, ADDR buf, tem1, ecx
				invoke szCatStr, edi, ADDR buf
				pop    edx
				pop    ecx
				add    edi, size1
			.else
				push   ecx
				push   edx
				xor    ecx, ecx
				mov    cl, BYTE PTR [esi]
				invoke wsprintf, ADDR buf, tem2, ecx
				invoke szCatStr, edi, ADDR buf
				pop    edx
				pop    ecx
				add    edi, size2
			.endif
			dec    ecx
			inc    edx
			inc    esi
			jmp    @B
		@exitB:
			.if wParam == 13009
				mov    BYTE PTR [edi+0], 34 ; "
				mov    BYTE PTR [edi+1], 0
			.else
				mov    BYTE PTR [edi-1], 0
			.endif
			invoke GlobalUnlock, hex_clip_copy
			invoke SetClipboardData, CF_TEXT, hex_clip_copy
			invoke CloseClipboard
		@exit__:
			invoke GlobalFree, hex_clip_copy
		.else
			; Forward unrecognized WM_COMMAND to parent window for custom menu items
			invoke GetParent, HM.hWnd[ebx]
			invoke SendMessage, eax, WM_COMMAND, wParam, lParam
		.endif
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==WM_SIZE
		.if lParam==0
			xor    eax, eax
			ret
		.endif
	@sizing:
		mov    eax, HM.view_lenx[ebx]
		mov    tem1, eax
		mov    eax, HM.view_leny[ebx]
		mov    tem2, eax
		
		mov    eax, lParam
		and    eax, 0ffffh
		;view_lenx = (XSize - (1+8+4) - foncx*8) / foncx*4
		sub    eax, 13
		mov    ecx, HM.fon_cx[ebx]
		shl    ecx, 3
		sub    eax, ecx
		cmp    eax, ecx
		jg     @F
		ret
		@@:
		shr    ecx, 1
		xor    edx, edx
		div    ecx
		.if HM.view_lenx[ebx] != eax && eax > 3
			mov    HM.view_lenx[ebx], eax
			;------
			mov    eax, HM.fon_cx[ebx]
			lea    eax, [eax+eax*2]
			mov    ecx, HM.view_lenx[ebx]
			mul    ecx
			add    eax, HM.rc2.left[ebx]
			mov    HM.rc2.right[ebx], eax
			add    eax, 4
			mov    HM.rc3.left[ebx], eax
			;------
			mov    eax, HM.fon_cx[ebx]
			mov    ecx, HM.view_lenx[ebx]
			mul    ecx
			add    eax, HM.rc3.left[ebx]
			mov    HM.rc3.right[ebx],    eax
			mov    HM.sel_rc.right[ebx], eax
		.endif
		mov    eax, lParam
		shr    eax, 16
		xor    edx, edx
		sub    eax, HM.rc1.top[ebx]
		cmp    eax, HM.fon_cy[ebx]
		jg     @F
		mov    eax, HM.fon_cy[ebx]
		@@:
		div    HM.fon_cy[ebx]
		mov    ecx, eax
		mul    HM.fon_cy[ebx]
		mov    edx, HM.view_leny[ebx]
		.if ecx != edx
			add    eax, HM.rc1.top[ebx]
			mov    HM.rc1.bottom[ebx],    eax
			mov    HM.rc2.bottom[ebx],    eax
			mov    HM.rc3.bottom[ebx],    eax
			mov    HM.sel_rc.bottom[ebx], eax
			mov    HM.view_leny[ebx],     ecx
		.endif
		
		cmp    HM.mdc[ebx], 0
		jz     @F
		invoke DeleteDC, HM.mdc[ebx]
		@@:
		invoke GetDC, HM.hWnd[ebx]
		mov    dc, eax
		invoke CreateCompatibleDC, eax
		mov    HM.mdc[ebx], eax
		mov    eax, lParam
		mov    ecx, eax
		shr    ecx, 16
		and    eax, 0ffffh
		mov    HM.main_rc.right[ebx],  eax
		mov    HM.main_rc.bottom[ebx], ecx
		invoke CreateCompatibleBitmap, dc, eax, ecx
		mov    hbit, eax
		invoke SelectObject, HM.mdc[ebx], eax
		invoke DeleteObject, eax
		invoke DeleteObject, hbit
		invoke ReleaseDC, HM.hWnd[ebx], dc
		invoke SelectObject, HM.mdc[ebx], HM.font[ebx]
		
		mov    sinfo.cbSize, sizeof SCROLLINFO
		mov    sinfo.fMask, SIF_DISABLENOSCROLL+SIF_PAGE+SIF_RANGE+SIF_POS
		mov    eax, HM.vscrollpos[ebx]
		mul    tem1
		xor    edx, edx
		div    HM.view_lenx[ebx]
		mov    sinfo.nPos, eax
		mov    HM.vscrollpos[ebx], eax
		
		mov    eax, HM.view_leny[ebx]
		mov    sinfo.nPage, eax
		mov    ecx, HM.view_lenx[ebx]
		mov    eax, HM._size[ebx]
		dec    eax
		xor    edx, edx
		div    ecx
		mov    sinfo.nMax,   eax
		mov    HM._nMax[ebx], eax
		
		cmp    eax, HM.view_leny[ebx]
		jbe    @F
		mov    ecx, sinfo.nPos
		add    ecx, HM.view_leny[ebx]
		cmp    ecx, eax
		jb     @F
		sub    eax, HM.view_leny[ebx]
		inc    eax
		mov    sinfo.nPos, eax
		mov    HM.vscrollpos[ebx], eax
		@@:
		xor    ecx, ecx
		mov    sinfo.nMin, ecx
		invoke SetScrollInfo, HM.hWnd[ebx], SB_VERT, ADDR sinfo, 1
		
		mov    ecx, tem1
		mov    edx, tem2
		cmp    ecx, HM.view_lenx[ebx]
		jnz    @set_cols
		cmp    edx, HM.view_leny[ebx]
		jnz    @set_cols
		jmp    @F
		@set_cols:
		invoke SetScreenColors
		@@:
		invoke DrawBase
		invoke DrawHeader
		invoke DrawOffset
		
		invoke InvalidateRect, HM.hWnd[ebx], 0, 0

;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==WM_PAINT
		invoke BeginPaint, HM.hWnd[ebx], ADDR ps
			
		invoke DrawOffset	
		invoke DrawHeader
		;-------------------------------------
		mov    esi, HM.view_lenx[ebx]
		;------
		mov    eax, ps.rcPaint.top
		sub    eax, HM.rc1.top[ebx]
		cmp    eax, 0
		jg     @F
		mov    eax, 0
		@@:
		xor    edx, edx
		div    HM.fon_cy[ebx]
		mul    esi
		mov    update_start, eax
		;------
		mov    eax, ps.rcPaint.bottom
		sub    eax, HM.rc1.top[ebx]
		cmp    eax, 0
		jg     @F
		mov    eax, 0
		@@:
		xor    edx, edx
		div    HM.fon_cy[ebx]
		mul    esi
		mov    update_end, eax
		;------
		;---STAGE_2-3-------------------------
		mov    eax, HM.fon_cx[ebx]
		lea    eax, [eax+eax*2]
		mov    cx_wid, eax
		
		mov    edi, HM.pointer[ebx]
		mov    eax, edi
		add    eax, HM._size[ebx]
		mov    tot_size, eax
		mov    eax, HM.vscrollpos[ebx]
		mul    esi
		add    edi, eax
		add    edi, update_start
		cmp    edi, tot_size
		jae    @no_empty
		
		mov    esi, update_start
		;-----------------------------------------------
	@@:	
		mov    eax, esi
		xor    edx, edx
		div    HM.view_lenx[ebx]
		mov    line_count,  edx
		mov    eax, HM.screen_colors_st2[ebx]
		mov    eax, [esi*8+0][eax]
		mov    col1a, eax
		mov    eax, HM.screen_colors_st2[ebx]
		mov    eax, [esi*8+4][eax]
		mov    col2a, eax
		mov    eax, HM.screen_colors_st3[ebx]
		mov    eax, [esi*8+0][eax]
		mov    col1b, eax
		mov    eax, HM.screen_colors_st3[ebx]
		mov    eax, [esi*8+4][eax]
		mov    col2b, eax
		mov    tem1, esi
		xor    edx, edx
	@str_cat:
		xor    ecx, ecx
		mov    cl, BYTE PTR [edi]
		lea    ecx, [ecx+ecx*2]
		push   edx
		mov    eax, edx
		lea    eax, [eax+eax*2]
		mov    dl, BYTE PTR [hexlook+ecx+0]
		mov    BYTE PTR [one_line1+eax+0], dl
		mov    dl, BYTE PTR [hexlook+ecx+1]
		mov    BYTE PTR [one_line1+eax+1], dl
		mov    dl, BYTE PTR [hexlook+ecx+2]
		mov    BYTE PTR [one_line1+eax+2], dl
		pop    edx
		mov    cl, BYTE PTR [edi]
		.if cl < 33 || cl > 126
			mov    cl, '.'
		.endif
		mov    BYTE PTR [one_line2+edx], cl
		inc    edx
		inc    line_count
		inc    esi
		inc    edi
		mov    eax, HM.view_lenx[ebx]
		cmp    line_count, eax
		jz     @str_out
		mov    eax, HM.screen_colors_st2[ebx]
		mov    ecx, [esi*8+4][eax]
		mov    eax, [esi*8+0][eax]
		cmp    edi, tot_size
		jz     @str_out
		cmp    eax, col1a
		jnz    @str_out
		cmp    ecx, col2a
		jnz    @str_out
		jmp    @str_cat
	@str_out:
		mov    line_len, edx
		;=================================================
		
		invoke SetBkColor,   HM.mdc[ebx], col1a
		invoke SetTextColor, HM.mdc[ebx], col2a
			
		mov    eax, tem1
		xor    edx, edx
		div    HM.view_lenx[ebx]
		mov    ecx, edx
		push   ecx
		mul    HM.fon_cy[ebx]
		add    eax, HM.rc1.top[ebx]
		mov    set_rc.top, eax
		add    eax, HM.fon_cy[ebx]
		mov    set_rc.bottom, eax
		
		mov    eax, cx_wid
		mul    ecx
		add    eax, HM.rc2.left[ebx]
		mov    set_rc.left, eax
		
		mov    eax, cx_wid
		mul    line_len
		add    eax, set_rc.left
		mov    set_rc.right, eax
		
		mov    ecx, line_len
		lea    ecx, [ecx+ecx*2]
		invoke ExtTextOut, HM.mdc[ebx], set_rc.left, set_rc.top, ETO_OPAQUE+ETO_CLIPPED, ADDR set_rc, ADDR one_line1, ecx, 0
		
		invoke SetBkColor,   HM.mdc[ebx], col1b
		invoke SetTextColor, HM.mdc[ebx], col2b
		
		pop    ecx
		
		mov    eax, HM.fon_cx[ebx]
		mul    ecx
		add    eax, HM.rc3.left[ebx]
		mov    set_rc.left, eax
		
		mov    eax, HM.fon_cx[ebx]
		mul    line_len
		add    eax, set_rc.left
		mov    set_rc.right, eax
		
		invoke ExtTextOut, HM.mdc[ebx], set_rc.left, set_rc.top, ETO_OPAQUE+ETO_CLIPPED, ADDR set_rc, ADDR one_line2, line_len, 0
			
		cmp    edi, tot_size
		jz     @no_empty
		cmp    esi, update_end
		jae    @no_empty
		jmp    @B
		
	@no_empty:
			
		cmp    edi, tot_size
		jb     @no_end
			
		mov    eax, set_rc.right
		mov    set_rc.left, eax
		mov    eax, HM.rc3.right[ebx]
		mov    set_rc.right, eax
		invoke SetBkColor, HM.mdc[ebx], [HM.bk_color+20][ebx]
		invoke ExtTextOut, HM.mdc[ebx], 0, 0, ETO_OPAQUE, ADDR set_rc, 0, 0, 0
			
		mov    eax, set_rc.left
		sub    eax, HM.rc3.left[ebx]
		xor    edx, edx
		div    HM.fon_cx[ebx]
		mul    cx_wid
		add    eax, HM.rc2.left[ebx]
		mov    set_rc.left, eax
		mov    eax, HM.rc2.right[ebx]
		mov    set_rc.right, eax
		invoke SetBkColor, HM.mdc[ebx], [HM.bk_color+16][ebx]
		invoke ExtTextOut, HM.mdc[ebx], 0, 0, ETO_OPAQUE, ADDR set_rc, 0, 0, 0
			
		@no_end:
			
		invoke BitBlt, ps.hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom, HM.mdc[ebx], ps.rcPaint.left, ps.rcPaint.top, SRCCOPY
			
		invoke EndPaint, HM.hWnd[ebx], ADDR ps
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==WM_ERASEBKGND
		mov eax, 1
		ret
;$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
	.elseif eax==WM_DESTROY
		invoke ClearUndoBuf
		invoke DeleteObject, [HM.lf_choose+ 0][ebx]
		invoke DeleteObject, [HM.lf_choose+ 4][ebx]
		invoke DeleteObject, [HM.lf_choose+ 8][ebx]
		invoke DeleteObject, [HM.lf_choose+12][ebx]
		invoke DeleteObject, [HM.lf_choose+16][ebx]
		invoke DeleteDC,      HM.mdc[ebx]
		invoke DestroyMenu,  [HM.hex_popupA][ebx]
		invoke DestroyMenu,  [HM.hex_popup][ebx]
      
		.if HM.find_hwnd[ebx]
			invoke DestroyWindow, HM.find_hwnd[ebx]
		.endif
		.if HM.select_hwnd[ebx]
			invoke DestroyWindow, HM.select_hwnd[ebx]
		.endif
	
	.elseif eax==WM_NCDESTROY
		invoke HeapFree, glob_heap, 0, HM.screen_colors_st2[ebx]
		invoke HeapFree, glob_heap, 0, HM.screen_colors_st3[ebx]
		invoke HeapFree, glob_heap, 0, ebx
		
	.else
		invoke DefWindowProc, HM.hWnd[ebx], uMsg, wParam, lParam
		ret
	.endif
	
	xor eax, eax
	ret

ShineInHexProc endp

ClearUndoBuf proc
	LOCAL pUndo:DWORD
		
	cmp    HM.undo_start[ebx], 0
	jz     @empty_buf	
	;-FIND BUFFER END
	mov    esi, HM.undo_start[ebx]
	@@:
	mov    edx, DWORD PTR [esi+12]
	test   edx, edx
	jz     @F
	mov    esi, edx
	jmp    @B
	;-FREE EVERYTHING
	@@:
	mov    edi, DWORD PTR [esi+16]
	mov    ecx, DWORD PTR [esi+ 0]
	invoke HeapFree, glob_heap, 0, ecx
	invoke HeapFree, glob_heap, 0, esi
	test   edi, edi
	jz     @empty_buf
	mov    esi, edi
	jmp    @B
	@empty_buf:
	
	ret
ClearUndoBuf endp

AllocUndo proc
	LOCAL pUndo:DWORD
		
	mov    HM.can_undo[ebx], 1
	invoke HeapAlloc, glob_heap, HEAP_GENERATE_EXCEPTIONS, sizeof UNDO
	mov    pUndo, eax
	mov    ecx, HM.undo_start[ebx]
	mov    edx, DWORD PTR [ecx+12]
	test   edx, edx
	jz     @F
	mov    DWORD PTR [edx+16], eax
	@@:
	mov    DWORD PTR [ecx+12], eax
	;------------------------------;
	;mov    DWORD PTR [eax+ 0], 0   ; un_pointer ; pointer to backuped data
	;mov    DWORD PTR [eax+ 4], 0   ; un_size    ; size of that data
	;mov    DWORD PTR [eax+ 8], 0   ; un_offset  ; original pointer of start data
	mov    DWORD PTR [eax+12], edx ; un_next
	mov    DWORD PTR [eax+16], ecx ; un_prev
	mov    HM.undo_start[ebx], eax
	
	mov    ecx, HM.sel_end[ebx]
	mov    edx, HM.sel_start[ebx]
	cmp    ecx, edx
	ja     @F
	xchg   ecx, edx
	@@:
	mov    esi, edx
	add    esi, HM.pointer[ebx]
	mov    DWORD PTR [eax+ 8], esi
	sub    ecx, edx
	test   ecx, ecx
	jnz    @F
	inc    ecx
	@@:
	mov    DWORD PTR [eax+ 4], ecx
	invoke HeapAlloc, glob_heap, HEAP_GENERATE_EXCEPTIONS, ecx
	mov    ecx, pUndo
	mov    DWORD PTR [ecx+0], eax
	mov    edi, eax
	mov    esi, DWORD PTR [ecx+8]
	mov    ecx, DWORD PTR [ecx+4]
	invoke MemCopy, esi, edi, ecx
		
	ret
AllocUndo endp

my_slow_cmp proc uses esi edi src:DWORD, dst:DWORD, len:DWORD
	
	mov    esi, src
	mov    edi, dst
	mov    ecx, len
	
	@@:
	mov    al, BYTE PTR [esi]
	mov    dl, BYTE PTR [edi]
	cmp    al, dl
	jnz    @no_match
	dec    ecx
	test   ecx, ecx
	jz     @match
	inc    esi
	inc    edi
	jmp    @B
	@match:
	mov    eax, 1
	ret
	@no_match:
	mov    eax, 0
	ret

my_slow_cmp endp

DoUndo proc
	LOCAL pUndo:DWORD
	LOCAL len:DWORD
	
	@duplicate:	
	cmp    HM.can_undo[ebx], 1
	jnz    @cannot_undo
	mov    eax, HM.undo_start[ebx]
	mov    eax, DWORD PTR [eax+16]
	mov    HM.undo_start[ebx], eax
	mov    esi, DWORD PTR [eax+0]
	mov    ecx, DWORD PTR [eax+4]
	mov    edi, DWORD PTR [eax+8]
	mov    edx, DWORD PTR [eax+16]
	mov    edx, DWORD PTR [edx+16]
	test   edx, edx
	jnz    @F
	mov    HM.can_undo[ebx], 0
	@@:
	mov    HM.can_redo[ebx], 1
	mov    len, ecx
	invoke my_slow_cmp, esi, edi, ecx
	test   eax, eax
	jnz    @duplicate
	invoke MemCopy, esi, edi, len
	@cannot_undo:
	
	ret
DoUndo endp

DoRedo proc
	LOCAL pUndo:DWORD
	LOCAL len:DWORD
	
	@duplicate:	
	cmp    HM.can_redo[ebx], 1
	jnz    @cannot_redo
	mov    eax, HM.undo_start[ebx]
	mov    esi, DWORD PTR [eax+0]
	mov    ecx, DWORD PTR [eax+4]
	mov    edi, DWORD PTR [eax+8]
	mov    edx, DWORD PTR [eax+12]
	test   edx, edx
	jnz    @F
	mov    HM.can_redo[ebx], 0
	jmp    @_
	@@:
	mov    HM.undo_start[ebx], edx
	@_:
	mov    HM.can_undo[ebx], 1
	mov    len, ecx
	invoke my_slow_cmp, esi, edi, ecx
	test   eax, eax
	jnz    @duplicate
	invoke MemCopy, esi, edi, len
	@cannot_redo:
	ret
DoRedo endp

InvalidateSel proc uses edi hWin:DWORD
	LOCAL rc:RECT
	LOCAL tem:DWORD


	invoke LockWindowUpdate, hWin
		
	mov    esi, HM.sel_last[ebx]
	mov    edi, HM.sel_end[ebx]
	cmp    esi, edi
	jb     @F
	xchg   esi, edi
	inc    edi
	@@:
	mov    eax, HM.view_lenx[ebx]
	mul    HM.vscrollpos[ebx]
	mov    ecx, eax
	cmp    eax, esi
	cmova  esi, eax
	cmp    edi, eax
	jb     @no_inv
	mov    eax, HM.view_lenx[ebx]
	mul    HM.view_leny[ebx]
	add    eax, ecx
	cmp    eax, edi
	cmovbe edi, eax
	sub    edi, ecx
	sub    esi, ecx
  @@:
	xor    edx, edx
	mov    eax, esi
	div    HM.view_lenx[ebx]
	mov    tem, edx
	mov    ecx, HM.fon_cy[ebx]
	mul    ecx
	add    eax, HM.rc1.top[ebx]
	mov    rc.top, eax
	add    eax, ecx
	mov    rc.bottom, eax
	mov    eax, HM.fon_cx[ebx]
	lea    eax, [eax+eax*2]
	mov    ecx, eax
	mul    tem
	add    eax, HM.rc2.left[ebx]
	mov    rc.left, eax
	add    eax, ecx
	mov    rc.right, eax
	invoke InvalidateRect, hWin, ADDR rc, 0
		
	mov    ecx, HM.fon_cx[ebx]
	mov    eax, ecx
	mul    tem
	add    eax, HM.rc3.left[ebx]
	mov    rc.left, eax
	add    eax, ecx
	mov    rc.right, eax
		
	invoke InvalidateRect, hWin, ADDR rc, 0
		
	inc    esi
	cmp    esi, edi
	jb     @B
		
	mov    rc.left, 0
	mov    rc.top,  0
	mov    eax, HM.fon_cy[ebx]
	mov    rc.bottom, eax
	mov    eax, HM.fon_cx[ebx]
	shl    eax, 3
	mov    rc.right, eax
	invoke InvalidateRect, hWin, ADDR rc, 0
		
	mov    eax, HM.rc3.left[ebx]
	mov    rc.left, eax
	mov    rc.top, 0
	mov    eax, HM.fon_cy[ebx]
	mov    rc.bottom, eax
	mov    eax, HM.fon_cx[ebx]
	shl    eax, 2
	add    eax, rc.left
	mov    rc.right, eax
	invoke InvalidateRect, hWin, ADDR rc, 0
		
	invoke LockWindowUpdate, 0


  @no_inv:
	
	ret
InvalidateSel endp

DrawBase proc
	invoke SetBkColor, HM.mdc[ebx], [HM.bk_color+16][ebx]
	invoke ExtTextOut, HM.mdc[ebx], 0, 0, ETO_OPAQUE, ADDR HM.main_rc[ebx], 0, 0, 0
	mov    esi, HM.rc1.top[ebx]
	dec    esi
	invoke MoveToEx, HM.mdc[ebx], 0, esi, 0
	invoke LineTo,   HM.mdc[ebx], HM.main_rc.right[ebx], esi
	
	inc    esi
	mov    edi, HM.rc1.right[ebx]
	invoke MoveToEx, HM.mdc[ebx], edi, esi, 0
	invoke LineTo,   HM.mdc[ebx], edi, HM.main_rc.bottom[ebx]
	
	ret
DrawBase endp

DrawHeader proc
	LOCAL vertex[2]:TRIVERTEX
	LOCAL mesh:GRADIENT_RECT
	LOCAL rc:RECT
	LOCAL buf[32]:BYTE

	lea		ecx, vertex
	mov		TRIVERTEX.x[ecx], 0
	mov		TRIVERTEX.y[ecx], 0
	mov		TRIVERTEX.Alpha[ecx], 0
	mov     edx, [HM.bk_color+4][ebx]
	mov     eax, edx
	shl     eax, 8
	mov		TRIVERTEX.Red[ecx],   ax
	mov     eax, edx
	and     eax, 0ff00h
	mov		TRIVERTEX.Green[ecx], ax
	mov     eax, edx
	shr     eax, 8
	and     eax, 0ff00h
	mov		TRIVERTEX.Blue[ecx],  ax
	
	add		ecx, sizeof TRIVERTEX
	m2m		TRIVERTEX.x[ecx], HM.main_rc.right[ebx]
	m2m		TRIVERTEX.y[ecx], HM.fon_cy[ebx]
	mov		TRIVERTEX.Alpha[ecx], 0
	mov     edx, [HM.text_color+4][ebx]
	mov     eax, edx
	shl     eax, 8
	mov		TRIVERTEX.Red[ecx],   ax
	mov     eax, edx
	and     eax, 0ff00h
	mov		TRIVERTEX.Green[ecx], ax
	mov     eax, edx
	shr     eax, 8
	and     eax, 0ff00h
	mov		TRIVERTEX.Blue[ecx],  ax

	mov		mesh.UpperLeft,  0
	mov	    mesh.LowerRight, 1
	
	invoke GradientFill, HM.mdc[ebx], ADDR vertex, 2, ADDR mesh, 1, GRADIENT_FILL_RECT_V
	invoke SetTextColor, HM.mdc[ebx], [HM.bk_color+12][ebx]
	invoke SetBkMode,    HM.mdc[ebx], TRANSPARENT
	mov    eax, HM.view_lenx[ebx]
	lea    eax, [eax+eax*2]
	invoke TextOut, HM.mdc[ebx], HM.rc2.left[ebx], 0, ADDR hexlook, eax
	
	mov    ecx, HM.sel_end[ebx]
	mov    edx, HM.sel_start[ebx]
	cmp    ecx, edx
	jz     @offset
	ja     @F
	xchg   ecx, edx
	@@:
	sub    ecx, edx
	@offset:
	invoke dw2hex, ecx, ADDR buf
	invoke SetTextColor, HM.mdc[ebx], [HM.bk_color+8][ebx]
	
	invoke TextOut, HM.mdc[ebx], 0, 0, ADDR buf, 8
	
	invoke SetTextColor, HM.mdc[ebx], [HM.text_color+12][ebx]
	
	mov    esi, OFFSET m2
	cmp    HM.input_mode[ebx], 1
	jz     @F
	mov    esi, OFFSET m1
	@@:
		
	invoke TextOut, HM.mdc[ebx], HM.rc3.left[ebx], 0, esi, 4
		
	ret
DrawHeader endp

DrawOffset proc
	LOCAL fold_curr:DWORD
	LOCAL set_rc:RECT
	LOCAL hexbuf[16]:BYTE
	LOCAL vertex[2]:TRIVERTEX
	LOCAL mesh:GRADIENT_RECT

	lea		ecx, vertex
	mov		TRIVERTEX.x[ecx], 0
	m2m		TRIVERTEX.y[ecx], HM.rc1.top[ebx]
	mov		TRIVERTEX.Alpha[ecx], 0
	mov     edx, [HM.bk_color+0][ebx]
	mov     eax, edx
	shl     eax, 8
	mov		TRIVERTEX.Red[ecx],   ax
	mov     eax, edx
	and     eax, 0ff00h
	mov		TRIVERTEX.Green[ecx], ax
	mov     eax, edx
	shr     eax, 8
	and     eax, 0ff00h
	mov		TRIVERTEX.Blue[ecx],  ax
	
	add		ecx, sizeof TRIVERTEX
	m2m		TRIVERTEX.x[ecx], HM.rc1.right[ebx]
	m2m		TRIVERTEX.y[ecx], HM.main_rc.bottom[ebx]
	mov		TRIVERTEX.Alpha[ecx], 0
	mov     edx, [HM.text_color+0][ebx]
	mov     eax, edx
	shl     eax, 8
	mov		TRIVERTEX.Red[ecx],   ax
	mov     eax, edx
	and     eax, 0ff00h
	mov		TRIVERTEX.Green[ecx], ax
	mov     eax, edx
	shr     eax, 8
	and     eax, 0ff00h
	mov		TRIVERTEX.Blue[ecx],  ax

	mov		mesh.UpperLeft,  0
	mov		mesh.LowerRight, 1
	
	invoke GradientFill, HM.mdc[ebx], ADDR vertex, 2, ADDR mesh, 1, GRADIENT_FILL_RECT_H
	
	invoke SetTextColor, HM.mdc[ebx], [HM.text_color+8][ebx]
	;___________________________
	mov    eax, HM.rc1.left[ebx]
	mov    set_rc.left, eax
	mov    eax, HM.rc1.right[ebx]
	mov    set_rc.right, eax
	mov    eax, HM.rc1.top[ebx]
	mov    set_rc.top,    eax
	add    eax, HM.fon_cy[ebx]
	mov    set_rc.bottom, eax
	;___________________________
	mov    eax, HM.vscrollpos[ebx]
	mul    HM.view_lenx[ebx]
	mov    edi, eax
	;___________________________
	
	xor    esi, esi
@@:
	mov    ecx, edi
	add    ecx, HM._start[ebx]
	invoke dw2hex, ecx, ADDR hexbuf
	add    edi, HM.view_lenx[ebx]
	
	invoke ExtTextOut, HM.mdc[ebx], set_rc.left, set_rc.top, ETO_CLIPPED, ADDR set_rc, ADDR hexbuf, 8, 0
	mov    eax, HM.fon_cy[ebx]
	add    set_rc.bottom, eax
	add    set_rc.top,    eax
		
	inc    esi
	cmp    esi, HM.view_leny[ebx]
	jnz    @B
	
	ret
DrawOffset endp

SetScreenColors proc uses esi edi
	LOCAL col1B:DWORD
	LOCAL col1T:DWORD
	LOCAL sc_start:DWORD
	LOCAL sc_end:DWORD

	mov    ecx, HM.pointer[ebx]
	mov    eax, HM.vscrollpos[ebx]
	mul    HM.view_lenx[ebx]
	add    ecx, eax    ; ecx points to first visible byte
	sub    ecx, HM.pointer[ebx]
	mov    sc_start, ecx
	mov    eax, HM.view_lenx[ebx]
	mul    HM.view_leny[ebx]
	add    ecx, eax
	mov    eax, HM._size[ebx]
	cmp    ecx, eax
	jb     @F
	mov    ecx, eax
  @@:
	mov    sc_end, ecx
	mov    ecx, sc_end
	sub    ecx, sc_start	

	; Reallocate screen color buffers as needed to support large window sizes
	; Buffer size in bytes = (screen_colors_len * 8 + 8) The "+ 8" avoids a
	; buffer overflow when the cursor moves 1 position past the last hex byte
	.if ecx > HM.screen_colors_len[ebx]
		mov    HM.screen_colors_len[ebx], ecx
		shl    ecx, 3
		add    ecx, 8 
		invoke HeapReAlloc, glob_heap, HEAP_GENERATE_EXCEPTIONS, HM.screen_colors_st2[ebx], ecx
		mov    HM.screen_colors_st2[ebx], eax

		mov    ecx, HM.screen_colors_len[ebx]
		shl    ecx, 3
		add    ecx, 8
		invoke HeapReAlloc, glob_heap, HEAP_GENERATE_EXCEPTIONS, HM.screen_colors_st3[ebx], ecx
		mov    HM.screen_colors_st3[ebx], eax

		mov    ecx, HM.screen_colors_len[ebx]
	.endif
	
	mov    esi, HM.screen_colors_st2[ebx]
	mov    edi, HM.screen_colors_st3[ebx]

	; HIGHLIGHTING OF THE LOWEST PRIO. - normal bk, normal text
  @@:
	dec    ecx
	mov    eax, ecx
	add    eax, sc_start
	mov    eax, [HM.bk_color  +16][ebx]
	mov    DWORD PTR [esi+ecx*8+0], eax
	mov    eax, [HM.bk_color  +20][ebx]
	mov    DWORD PTR [edi+ecx*8+0], eax
	mov    eax, [HM.text_color+16][ebx]
	mov    DWORD PTR [esi+ecx*8+4], eax
	mov    eax, [HM.text_color+20][ebx]
	mov    DWORD PTR [edi+ecx*8+4], eax
	test   ecx, ecx
	jnz    @B
	; HIGHLIGHTING OF BIGGER PRIO. - modified bytes
	mov    ecx, HM.modified_counter[ebx]
  @@:
	test   ecx, ecx
	jz     @F
	;xor    edx, edx
	dec    ecx
	mov    eax, ecx
	shr    eax, 3
	mov    edi, [HM.modified_bytes+ecx*4][ebx]
	cmp    edi, sc_end
	ja     @next_byte
	cmp    edi, sc_start
	jb     @next_byte
	sub    edi, sc_start
	mov    edx, [HM.bk_color  +eax*4+36][ebx]
	mov    DWORD PTR [esi+edi*8+0], edx
	mov    edx, [HM.text_color+eax*4+36][ebx]
	mov    DWORD PTR [esi+edi*8+4], edx
  @next_byte:
	jmp    @B
	
  @@:
	; Send HEXN_CUSTOMCOLORS notify so parent can customize color buffers
	mov    edx, HM._start[ebx]
	mov    HM.nm.hdr.code[ebx], HEXN_CUSTOMCOLORS
	m2m    HM.nm.colors.colors_hex[ebx], HM.screen_colors_st2[ebx]
	m2m    HM.nm.colors.colors_ansi[ebx], HM.screen_colors_st3[ebx]
	m2m    HM.nm.colors.sc_start[ebx], sc_start
	add    HM.nm.colors.sc_start[ebx], edx
	m2m    HM.nm.colors.sc_end[ebx], sc_end
	add    HM.nm.colors.sc_end[ebx], edx
	invoke SendNotify
	
	; HIGHLIGHTING OF BIGGER PRIO. - cursor pos
	mov    esi, HM.screen_colors_st2[ebx]
	mov    edi, HM.screen_colors_st3[ebx]
	mov    ecx, HM.sel_start[ebx]
	mov    edx, HM.sel_end[ebx]
	cmp    ecx, edx
	jnz    @F
	cmp    edx, sc_end
	ja     @no_sel
	cmp    edx, sc_start
	jb     @no_sel
	sub    edx, sc_start
	mov    eax, [HM.bk_color  +32][ebx]
	mov    DWORD PTR [esi+edx*8+0], eax
	mov    DWORD PTR [edi+edx*8+0], eax
	mov    eax, [HM.text_color+32][ebx]
	mov    DWORD PTR [esi+edx*8+4], eax
	mov    DWORD PTR [edi+edx*8+4], eax
	jmp    @no_sel
	; HIGHLIGHTING OF THE BIGGEST PRIO. - selection
  @@:
	cmp    ecx, edx
	jb     @F
	xchg   ecx, edx
  @@:
	cmp    ecx, sc_end
	ja     @no_sel
	cmp    edx, sc_start
	jb     @no_sel
	cmp    ecx, sc_start
	ja     @F
	mov    ecx, sc_start
  @@:	
	cmp    ecx, edx
	jz     @no_sel
	cmp    ecx, sc_end
	jz     @no_sel
	push   ecx
	sub    ecx, sc_start
	mov    eax, [HM.bk_color  +24][ebx]
	mov    DWORD PTR [esi+ecx*8+0], eax
	mov    eax, [HM.bk_color  +28][ebx]
	mov    DWORD PTR [edi+ecx*8+0], eax
	mov    eax, [HM.text_color+24][ebx]
	mov    DWORD PTR [esi+ecx*8+4], eax
	mov    eax, [HM.text_color+28][ebx]
	mov    DWORD PTR [edi+ecx*8+4], eax
	pop    ecx
	inc    ecx
	jmp    @B
  @no_sel:	
	;-----------------
	ret
SetScreenColors endp

to_UTF8 proc uses esi edi ebx sour:DWORD,dest:DWORD
	LOCAL b1:BYTE
	LOCAL b2:BYTE
	
	xor    eax,eax
	mov    esi,sour
	mov    edi,dest
@@:
	mov    ax,word ptr [esi]
	;cmp    ax,0FFFFh
	;jg     _4bytes
	cmp    ax,0
	je     fin
	cmp    ax,07FFh
	jg     _3bytes
	cmp    ax,007Fh
	jg     _2bytes
	;----1_byte
	mov    BYTE PTR [edi],al
	inc    edi
	jmp    next_char
_2bytes:
	mov    ebx,080h
	mov    ecx,eax
	and    ecx,3Fh
	or     ecx,ebx
	mov    edx,ecx
	;---------------------
	mov    ebx,0C0h
	mov    ecx,eax
	shr    ecx,6
	or     ecx,ebx
	;---------------------
	mov    BYTE PTR [edi],cl
	mov    BYTE PTR [edi+1],dl
	add    edi,2
	jmp    next_char
_3bytes:
	mov    ebx,080h
	mov    ecx,eax
	and    ecx,3Fh
	or     ecx,ebx
	mov    b1,cl
	;--------------
	mov    ebx,080h
	mov    ecx,eax
	and    ecx,0FFFh
	shr    ecx,6
	or     ecx,ebx
	mov    b2,cl
	;--------------
	mov    ebx,0E0h
	mov    ecx,eax
	shr    ecx,12
	or     ecx,ebx
	;--------------
	mov    BYTE PTR [edi],cl
	mov    cl,b2
	mov    BYTE PTR [edi+1],cl
	mov    cl,b1
	mov    BYTE PTR [edi+2],cl
	add    edi,3
_4bytes: ;WE'LL_IGNORE_IT_AS_MOST_OF_API_DOES
next_char:
	add    esi,2
	jmp    @B
fin:
	mov    BYTE PTR [edi],0	
	
	ret

to_UTF8 endp

DlgFind proc uses esi edi hDlg:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
	LOCAL buf1[512]:BYTE
	LOCAL buf2[512]:BYTE
	LOCAL tem[4]:BYTE
	LOCAl len:DWORD
	LOCAL reversed:DWORD
	LOCAL count:DWORD

	mov    eax, uMsg
	.if eax == WM_INITDIALOG
		mov    checked,    1
		mov    found_offs, 0
		invoke CheckDlgButton, hDlg, 702, BST_CHECKED
		invoke SetWindowLong, hDlg, GWL_USERDATA, lParam
		invoke SetLayeredWindowAttributes, hDlg, 0, 255, LWA_ALPHA
		invoke ShowWindow, hDlg, SW_SHOW
		mov    eax, lParam
		mov    ecx, hDlg
		mov    HM.find_hwnd[eax], ecx
	.elseif eax == WM_COMMAND
		invoke IsDlgButtonChecked, hDlg, 705
		mov    reversed, eax
		mov    eax, wParam
		.if eax == 702
			mov    checked, 1
			invoke CheckDlgButton, hDlg, 702, BST_CHECKED
			invoke CheckDlgButton, hDlg, 703, BST_UNCHECKED
			invoke CheckDlgButton, hDlg, 704, BST_UNCHECKED
		.elseif eax == 703
			mov    checked, 2
			invoke CheckDlgButton, hDlg, 703, BST_CHECKED
			invoke CheckDlgButton, hDlg, 702, BST_UNCHECKED
			invoke CheckDlgButton, hDlg, 704, BST_UNCHECKED
		.elseif eax == 704
			mov    checked, 3
			invoke CheckDlgButton, hDlg, 704, BST_CHECKED
			invoke CheckDlgButton, hDlg, 702, BST_UNCHECKED
			invoke CheckDlgButton, hDlg, 703, BST_UNCHECKED
		.elseif eax == 706
			invoke SendDlgItemMessageW, hDlg, 701, WM_GETTEXTLENGTH, 0, 0
			test   eax, eax
			jnz    @F
			ret
			@@:
			mov   eax, checked
			.if eax == 1
				invoke SendDlgItemMessageW, hDlg, 701, WM_GETTEXT, 512, ADDR buf1
				.if reversed
					invoke ucCopy, ADDR buf1, ADDR buf2
					invoke ucRev,  ADDR buf2, ADDR buf1
				.endif
				invoke to_UTF8, ADDR buf1, ADDR buf2
				invoke szLen, ADDR buf2
			.elseif eax == 2
				invoke SendDlgItemMessageW, hDlg, 701, WM_GETTEXT, 512, ADDR buf2
				.if reversed
					invoke ucCopy, ADDR buf2, ADDR buf1
					invoke ucRev,  ADDR buf1, ADDR buf2
				.endif
				invoke ucLen, ADDR buf2
				shl    eax, 1
			.elseif eax == 3
				invoke SendDlgItemMessage, hDlg, 701, WM_GETTEXT, 512, ADDR buf1
				mov    ecx, eax
				shr    ecx, 1
				shl    ecx, 1
				.if ecx != eax
					invoke MessageBox, hDlg, ADDR error_hex, 0, MB_OK
					ret
				.else
					mov    len, eax
					.if reversed
						mov    ecx, eax
						shr    ecx, 2
						lea    esi, buf1
						mov    edi, esi
						add    edi, eax
						@@:
						test   ecx, ecx
						jz     @F
						sub    edi, 2
						mov    ax, WORD PTR [esi]
						mov    dx, WORD PTR [edi]
						mov    WORD PTR [esi], dx
						mov    WORD PTR [edi], ax
						dec    ecx
						add    esi, 2
						jmp    @B
						@@:
					.endif
					lea    edi, buf2
					mov    esi, 0
					@@:	
					invoke szMid, ADDR buf1, ADDR tem, esi, 2
					invoke htodw, ADDR tem
					mov    BYTE PTR [edi], al
					add    esi, 2
					inc    edi
					cmp    esi, len
					jb     @B
				.endif
				mov     eax, len
				shr     eax, 1
			.endif
			mov    len, eax
			invoke GetWindowLong, hDlg, GWL_USERDATA
			mov    esi, eax
			
			@@:	
			mov    ecx, len
			inc    ecx
			mov    edx, HM._size[esi]
			inc    edx
			invoke BinSearch, found_offs, HM.pointer[esi], edx, ADDR buf2, ecx
			.if eax == -1
				mov    ecx, found_offs
				mov    found_offs, 0			
				.if ecx > 0
					jmp    @B
				.endif
				ret
			.endif
			mov    ecx, eax
			mov    found_offs, eax
			add    found_offs, 1
			add    ecx, len
			invoke SendMessage, HM.hWnd[esi], HEXM_SETSEL, eax, ecx
		.elseif eax == 707
			invoke IsDlgButtonChecked, hDlg, 707
			.if eax
				invoke SetLayeredWindowAttributes, hDlg, 0, 122, LWA_ALPHA
			.else
				invoke SetLayeredWindowAttributes, hDlg, 0, 255, LWA_ALPHA
			.endif
		.elseif eax == 708
			invoke EndDialog, hDlg, 0
		.else
			xor	eax, eax
			ret
		.endif
	.elseif eax == WM_CLOSE
		invoke EndDialog, hDlg, 0
	.else
		xor	eax, eax
		ret
	.endif
	mov	eax, 1
	ret
DlgFind endp

DlgSelect proc hDlg:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
	LOCAL s_offs[12]:BYTE
	LOCAL e_offs[12]:BYTE
	LOCAL S_OFFS:DWORD
	LOCAL E_OFFS:DWORD
	
	mov    eax, uMsg
	.if eax == WM_INITDIALOG
		invoke SetWindowLong, hDlg, GWL_USERDATA, lParam
		invoke ShowWindow, hDlg, SW_SHOW
		mov    eax, lParam
		mov    ecx, hDlg
		mov    HM.select_hwnd[eax], ecx
	.elseif eax == WM_COMMAND
		.if wParam == 805
			invoke SendDlgItemMessage, hDlg, 801, WM_GETTEXT, 9, ADDR s_offs
			invoke SendDlgItemMessage, hDlg, 802, WM_GETTEXT, 9, ADDR e_offs
			invoke htodw, ADDR s_offs
			mov    S_OFFS, eax
			invoke htodw, ADDR e_offs
			mov    E_OFFS, eax
			invoke GetWindowLong, hDlg, GWL_USERDATA
			invoke SendMessage, HM.hWnd[eax], HEXM_SETSEL, S_OFFS, E_OFFS
			invoke EndDialog, hDlg, 0
		.elseif wParam == 806
			invoke EndDialog, hDlg, 0
		.endif
	.elseif eax == WM_CLOSE
		invoke EndDialog, hDlg, 0
	.else
		xor    eax, eax
		ret
	.endif
	mov    eax, 1
	ret
DlgSelect endp

SendNotify proc
	mov    eax, HM.hWnd[ebx]
	mov    HM.nm.hdr.hwndFrom[ebx], eax
	invoke GetDlgCtrlID, eax
	mov    HM.nm.hdr.idFrom[ebx], eax
	invoke GetParent, HM.hWnd[ebx]
	mov    edx, eax
	invoke SendMessage, edx, WM_NOTIFY, HM.hWnd[ebx], ADDR HM.nm[ebx]
	ret
SendNotify endp

End DllEntry