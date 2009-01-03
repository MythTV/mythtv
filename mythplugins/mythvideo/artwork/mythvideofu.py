#!/usr/bin/python

from gimpfu import *

def ShadowLayer(image, layer):
	image.resize(image.width + 16, image.height + 16, 1, 1)
	pdb.script_fu_drop_shadow(image, layer, 8, 8, 15, (0, 0, 0), 60,
			False)

def MythVideoPopup(width, height, gimpFilename, pngFilename, autosave,
		interactive):
	if len(gimpFilename) and gimpFilename[-4:].lower() != ".xcf":
		gimpFilename += ".xcf"

	if len(pngFilename) and pngFilename[-4:].lower() != ".png":
		pngFilename += ".png"

	borderWidth = 5
	imageWidth = width + 2 * borderWidth
	imageHeight = height + 2 * borderWidth
	image = gimp.Image(imageWidth, imageHeight, RGB)

	image.disable_undo()
	gimp.context_push()

	layerBackground = gimp.Layer(image, "background", imageWidth,
			imageHeight, RGBA_IMAGE, 100, NORMAL_MODE)
	image.add_layer(layerBackground, 1)

	# white rect
	pdb.gimp_edit_clear(layerBackground)
	gimp.set_foreground((255, 255, 255))
	pdb.script_fu_selection_rounded_rectangle(image, layerBackground,
			14.0, False)
	pdb.gimp_edit_fill(layerBackground, BACKGROUND_FILL)
	pdb.script_fu_add_bevel(image, layerBackground, borderWidth,
			False, False)
	pdb.plug_in_antialias(image, layerBackground)

	# black rect
	layerBlackBox = gimp.Layer(image, "black box", imageWidth,
			imageHeight, RGBA_IMAGE, 100, NORMAL_MODE)
	image.add_layer(layerBlackBox, -1)
	pdb.gimp_edit_clear(layerBlackBox)
	pdb.script_fu_selection_rounded_rectangle(image, layerBlackBox,
			14.0, False)
	pdb.gimp_selection_shrink(image, borderWidth)
	gimp.set_foreground((0, 0, 0))
	pdb.gimp_edit_fill(layerBlackBox, FOREGROUND_FILL)
	pdb.plug_in_antialias(image, layerBlackBox)

	pdb.gimp_selection_layer_alpha(layerBackground)

	ShadowLayer(image, layerBackground)

	gimp.context_pop()
	image.enable_undo()

	pdb.gimp_selection_none(image)

	if len(gimpFilename):
		image.filename = gimpFilename

	if interactive:
		gimp.Display(image)

	if len(gimpFilename) and len(pngFilename) and autosave:
		pdb.gimp_file_save(image, layerBackground, gimpFilename,
				gimpFilename)

#		layerFlat = pdb.gimp_image_flatten(image)
		layerFlat = pdb.gimp_image_merge_visible_layers(image, CLIP_TO_IMAGE)

		pdb.file_png_save2(
				image,
				layerFlat,
				pngFilename, pngFilename,
				False, # adam7
				9,
				0, # bKGD
				0, # gAMA
				0, # oFFs
				0, # pHYs
				0, # tIME
				0, # comment
				0) # trans

	return image

def mythvideo_popup(bb_width, bb_height, gimpFilename, pngFilename,
		autosave, interactive):
	MythVideoPopup(bb_width, bb_height, gimpFilename, pngFilename, autosave,
			interactive)
	if interactive:
		gimp.displays_flush()

register("mythvideo-popup",
		"MythVideo Popup",
		"Create a basic MythVideo popup box",
		"",
		"",
		"",
		"Create Popup",
		"",
		[
			(PF_SPINNER, "bb_width", "Pre-border width", 400, (0, 1920, 50)),
			(PF_SPINNER, "bb_height", "Pre-border height", 200, (0, 1200, 50)),
			(PF_STRING, "gimp_name", "GIMP filename", ""),
			(PF_STRING, "png_name", "PNG export filename", ""),
			(PF_BOOL, "autosave", "Save", False),
			(PF_BOOL, "interactive", "interactive", True),
		],
		[],
		mythvideo_popup,
        menu="<Toolbox>/Xtns/MythTV/MythVideo")

main()
