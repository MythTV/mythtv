/*
 * This file is part of libbluray
 * Copyright (C) 2012  libbluray
 * Copyright (C) 2012  Petri Hintukainen <phintuka@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

package java.awt;

import java.awt.peer.KeyboardFocusManagerPeer;
import java.awt.peer.RobotPeer;

import sun.awt.ComponentFactory;
import sun.awt.KeyboardFocusManagerPeerProvider;
import sun.awt.datatransfer.DataTransferer;

import java.awt.peer.BDFramePeer;
import java.awt.peer.BDKeyboardFocusManagerPeer;

import org.videolan.GUIManager;
import org.videolan.Logger;

public class BDToolkit extends BDToolkitBase
    implements KeyboardFocusManagerPeerProvider, ComponentFactory {

    private static final Logger logger = Logger.getLogger(BDToolkit.class.getName());

    public BDToolkit () {
    }

    protected void shutdown() {
        super.shutdown();

        KeyboardFocusManager.getCurrentKeyboardFocusManager().clearGlobalFocusOwner();
        KeyboardFocusManager.getCurrentKeyboardFocusManager().setGlobalCurrentFocusCycleRoot(null);
        KeyboardFocusManager.getCurrentKeyboardFocusManager().setGlobalFocusedWindow(null);
        KeyboardFocusManager.getCurrentKeyboardFocusManager().setGlobalActiveWindow(null);
        KeyboardFocusManager.getCurrentKeyboardFocusManager().setGlobalPermanentFocusOwner(null);

        BDKeyboardFocusManagerPeer.shutdown();

        KeyboardFocusManager.setCurrentKeyboardFocusManager(null);
    }

    public static void setFocusedWindow(Window window) {
        /* hook KeyboardFocusManagerPeer (not doing this leads to crash) */
        BDKeyboardFocusManagerPeer.init(window);
    }

    // required by older Java 7 versions
    public KeyboardFocusManagerPeer createKeyboardFocusManagerPeer(KeyboardFocusManager kfm)
    {
        return getKeyboardFocusManagerPeer();
    }

    public KeyboardFocusManagerPeer getKeyboardFocusManagerPeer()
    {
        return BDKeyboardFocusManagerPeer.getInstance();
    }

    public void sync() {
        GUIManager.getInstance().sync();
    }

    public java.util.Map mapInputMethodHighlight(java.awt.im.InputMethodHighlight h) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public  boolean isModalExclusionTypeSupported(Dialog.ModalExclusionType modalExclusionType) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public  boolean isModalityTypeSupported(Dialog.ModalityType modalityType) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.FramePeer createFrame(Frame target) {
        if (!(target instanceof BDRootWindow)) {
            logger.error("createFrame(): not BDRootWindow");
            throw new Error("Not implemented");
        }
        return new BDFramePeer((BDRootWindow)target);
    }

    public java.awt.peer.ButtonPeer createButton(Button target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.CanvasPeer createCanvas(Canvas target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.CheckboxPeer createCheckbox(Checkbox target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.CheckboxMenuItemPeer createCheckboxMenuItem(CheckboxMenuItem target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.ChoicePeer createChoice(Choice target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.DesktopPeer createDesktopPeer(Desktop target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.DialogPeer createDialog(Dialog target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.dnd.peer.DragSourceContextPeer createDragSourceContextPeer(java.awt.dnd.DragGestureEvent dge) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.FileDialogPeer createFileDialog(FileDialog target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.LabelPeer createLabel(Label target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.ListPeer createList(List target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.MenuPeer createMenu(Menu target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.MenuBarPeer createMenuBar(MenuBar target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.MenuItemPeer createMenuItem(MenuItem target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.PanelPeer createPanel(Panel target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.PopupMenuPeer createPopupMenu(PopupMenu target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.ScrollbarPeer createScrollbar(Scrollbar target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.ScrollPanePeer createScrollPane(ScrollPane target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.TextAreaPeer createTextArea(TextArea target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.TextFieldPeer createTextField(TextField target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.WindowPeer createWindow(Window target) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.datatransfer.Clipboard getSystemClipboard() {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public PrintJob getPrintJob(Frame frame, String jobtitle, java.util.Properties props)  {
        logger.unimplemented();
        throw new Error("Not implemented");
    }
    public java.awt.peer.FontPeer getFontPeer(String name, int style) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }

    /* required for Java < 9 */
    public DataTransferer getDataTransferer() {
        return null;
    }
    public RobotPeer createRobot(Robot target, GraphicsDevice screen) {
        logger.unimplemented();
        throw new Error("Not implemented");
    }

}
