; This script created a basic MythVideo dialog and 
; selection bars.

; adds standard shadow to the image
(define (add-mv-shadow image layer)
  (let* (
         (width (car (gimp-image-width image)))
         (height (car (gimp-image-height image))))
    (gimp-image-resize image (+ width 16) (+ height 16) 1 1)
    (script-fu-drop-shadow image layer 8 8 15 '(0 0 0) 60 FALSE)))

; bb-width = black box widht
(define (script-fu-mv-dialog bb-width
                             bb-height
                             create-selbar
                             base-filename)
  (let* (
         (border-width 5)
         (width (+ bb-width (* border-width 2)))
         (height (+ bb-height (* border-width 2)))
         (image (car (gimp-image-new width height RGB)))
         (bg-layer (car (gimp-layer-new image width height RGBA-IMAGE
                                        "background" 100 NORMAL-MODE)))
         (arrow-width 15)
         (bar-end-to-arrow-width 12)
         (dialog-content-spacing 4)
         ; distance from the end of the arrow image to the border
         ; defined by dialog-content-spacing
         (end-arrow-to-border-width (+ dialog-content-spacing 10)))

    (gimp-context-push)
    (gimp-image-undo-disable image)
    (gimp-selection-none image)

    ; draw white rect layer
    (gimp-image-add-layer image bg-layer 1)
    (gimp-edit-clear bg-layer)
    (gimp-selection-none image)
    (script-fu-selection-rounded-rectangle image bg-layer 14.0 FALSE)
    (gimp-context-set-foreground '(255 255 255))
    (gimp-edit-fill bg-layer FOREGROUND-FILL)
    (script-fu-add-bevel image bg-layer border-width FALSE FALSE)
    (plug-in-antialias RUN-NONINTERACTIVE image bg-layer)

    (let* (
           (tmp-draw (car (gimp-layer-new image width height RGBA-IMAGE
                                          "black box" 100 NORMAL-MODE))))
      ; draw black rect layer
      (gimp-image-add-layer image tmp-draw -1)
      (gimp-edit-clear tmp-draw)
      (gimp-selection-none image)
      (script-fu-selection-rounded-rectangle image tmp-draw 14.0 FALSE)
      (gimp-selection-shrink image border-width)
      (gimp-context-set-foreground '(0 0 0))
      (gimp-edit-fill tmp-draw FOREGROUND-FILL)
      (plug-in-antialias RUN-NONINTERACTIVE image tmp-draw))

    (gimp-selection-none image)
    (gimp-selection-layer-alpha bg-layer)
    (add-mv-shadow image bg-layer)

    (if (= create-selbar TRUE)
      (let* (
             ; offset 4px from from bb, allowing 8px to scrollbar,
             ; its width, plus 4 to the other side
             (sel-width (- bb-width (+ (* 2 dialog-content-spacing) ; border
                                       (+ (+ arrow-width bar-end-to-arrow-width)
                                          end-arrow-to-border-width))))
             (sel-height 32) ; assumes 16pt font
             (sel-image (car (gimp-image-new sel-width sel-height RGB)))
             (sel-bg (car (gimp-layer-new sel-image sel-width sel-height
                                          RGBA-IMAGE "background" 100
                                          NORMAL-MODE))))
        (gimp-image-undo-disable sel-image)
        (gimp-image-add-layer sel-image sel-bg 1)
        (gimp-edit-clear sel-bg)
        (gimp-selection-none sel-image)
        (script-fu-selection-rounded-rectangle sel-image sel-bg 60.0 FALSE)
        (gimp-context-set-foreground '(18 0 255))
        (gimp-context-set-background '(126 121 234))
        (gimp-edit-blend sel-bg ; layer
                         FG-BG-RGB-MODE ; blend-mode
                         NORMAL-MODE ; paint-mode
                         GRADIENT-LINEAR ; gradient-type
                         90.0 ; opacity
                         0 ; offset
                         REPEAT-NONE ; repeat
                         FALSE ; reverse
                         TRUE ; supersample
                         1 ; max-depth
                         0 ; threshold
                         TRUE ; dither
                         0 0 ; x,y start
                         0 28 ; x,y end
                         )
        (script-fu-add-bevel sel-image sel-bg 2 FALSE FALSE)
        (plug-in-antialias RUN-NONINTERACTIVE sel-image sel-bg)

        (if (not (string=? base-filename ""))
          (gimp-image-set-filename sel-image
                                   (string-append base-filename
                                                  "-SelctionBar.xcf")))
        (gimp-selection-none sel-image)
        (gimp-image-undo-enable sel-image)
        (gimp-display-new sel-image)))

     (if (not (string=? base-filename ""))
       (gimp-image-set-filename image (string-append base-filename ".xcf")))

    (gimp-selection-none image)
    (gimp-image-undo-enable image)
    (gimp-display-new image)
    (gimp-context-pop)))

(define (script-fu-mv-trans-dialog bb-width
                                   bb-height
                                   base-filename)
  (let* (
          (border-width 8)
          (trans-border-width (- border-width 2))
          (width (+ bb-width (* border-width 2)))
          (height (+ bb-height (* border-width 2)))
          (image (car (gimp-image-new width height RGB)))
          (bg-layer (car (gimp-layer-new image width height RGBA-IMAGE
                                         "background" 100 NORMAL-MODE))))
    (gimp-context-push)
    (gimp-image-undo-disable image)

    (gimp-image-add-layer image bg-layer 1)
    (gimp-edit-clear bg-layer)
    (gimp-context-set-foreground '(0 0 0))
    (gimp-selection-none image)
    (script-fu-selection-rounded-rectangle image bg-layer 14.0 FALSE)
    (gimp-bucket-fill bg-layer FG-BUCKET-FILL NORMAL-MODE 70.0 0 FALSE 0 0)
    (gimp-selection-shrink image (- border-width trans-border-width))
    (gimp-edit-clear bg-layer)
    (gimp-bucket-fill bg-layer FG-BUCKET-FILL NORMAL-MODE 50.0 0 FALSE 0 0)
    (gimp-selection-shrink image trans-border-width)
    (gimp-edit-clear bg-layer)
    (gimp-bucket-fill bg-layer FG-BUCKET-FILL NORMAL-MODE 60.0 0 FALSE 0 0)

    ; add shadow
    (gimp-selection-none image)
    (script-fu-selection-rounded-rectangle image bg-layer 14.0 FALSE)
    (add-mv-shadow image bg-layer)

    (gimp-selection-none image)
    (plug-in-antialias RUN-NONINTERACTIVE image bg-layer)

    (if (not (string=? base-filename ""))
      (gimp-image-set-filename image (string-append base-filename ".xcf")))

    (gimp-selection-none image)
    (gimp-image-undo-enable image)
    (gimp-display-new image)
    (gimp-context-pop)))

(define (script-fu-mv-dialog-default bb-width bb-height create-selbar)
  (script-fu-mv-dialog bb-width bb-height create-selbar ""))

; SF-ADJUSTMENT (start min max small-step large-step [0 = int 1 = float]
; [0 = slider 1 = rollbox])
(script-fu-register "script-fu-mv-dialog-default" ; fn
					"MythVideo Dialog" ; menu label
					"Create a basic MythVideo dialog box"
					"" ; author
					"" ; copyright
					"" ; date
					"" ; image type
					SF-ADJUSTMENT "Black box width" '(400 1 1280 50 200 0 1)
					SF-ADJUSTMENT "Black box height" '(225 1 720 50 200 0 1)
					SF-TOGGLE "Create select bar" FALSE) 

(script-fu-menu-register "script-fu-mv-dialog-default"
                         "<Toolbox>/Xtns/Script-Fu/MythTV/MythVideo Dialog")

(define (script-fu-mv-create-all create-mst
                                 create-msu
                                 create-ss
                                 create-wait
                                 create-wide-too)
  ( let* (
          (mst-title "MythVideo-ManualSearchTitle")
          (msu-title "MythVideo-ManualSearchUID")
          (ss-title "MythVideo-SearchSelect")
          (wait-title "MythVideo-Wait"))

         (define (create-small)
           ; Small
           (if (= create-mst TRUE)
             (script-fu-mv-dialog 366 136 FALSE mst-title))
           (if (= create-msu TRUE)
             (script-fu-mv-dialog 341 130 FALSE msu-title))
           (if (= create-ss TRUE)
             (script-fu-mv-dialog 387 400 TRUE ss-title))
           (if (= create-wait TRUE)
             (script-fu-mv-trans-dialog 590 440 wait-title)))

         (create-small)

         (define (create-wide)
           ; Wide
           (if (= create-ss TRUE)
             (script-fu-mv-dialog 592 400 TRUE
                                  (string-append ss-title "-Wide")))
           (if (= create-wait TRUE)
             (script-fu-mv-trans-dialog 950 530 
                                        (string-append wait-title "-Wide"))))

         (if (= create-wide-too TRUE)
           (create-wide))))

; SF-ADJUSTMENT (start min max small-step large-step [0 = int 1 = float]
; [0 = slider 1 = rollbox])
(script-fu-register "script-fu-mv-create-all" ; fn
					"Create All" ; menu label
					"Create all basic MythVideo dialogs and selection bars"
					"" ; author
					"" ; copyright
					"" ; date
					"" ; image type
					SF-TOGGLE "Manual Search Title Small" TRUE
					SF-TOGGLE "Manual Search UID Small" TRUE
					SF-TOGGLE "Search Select Small" TRUE
					SF-TOGGLE "Wait Small" TRUE
					SF-TOGGLE "Also Create Wide Versions" TRUE) 

(script-fu-menu-register "script-fu-mv-create-all"
                         "<Toolbox>/Xtns/Script-Fu/MythTV/MythVideo Dialog")

; vim: ts=2 sw=2 expandtab :
