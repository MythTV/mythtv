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

import sun.awt.KeyboardFocusManagerPeerProvider;

import java.awt.peer.BDFramePeer;
import java.awt.peer.BDKeyboardFocusManagerPeer;

public class BDToolkit extends BDToolkitBase implements KeyboardFocusManagerPeerProvider {
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
        org.videolan.GUIManager.getInstance().sync();
    }

    public java.util.Map mapInputMethodHighlight(java.awt.im.InputMethodHighlight h) {
        throw new Error("Not implemented");
    }
    public  boolean isModalExclusionTypeSupported(Dialog.ModalExclusionType modalExclusionType) {
        throw new Error("Not implemented");
    }
    public  boolean isModalityTypeSupported(Dialog.ModalityType modalityType) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.FramePeer createFrame(Frame target) {
        return new BDFramePeer(target, (BDRootWindow)target);
    }

    protected java.awt.peer.ButtonPeer createButton(Button target) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.CanvasPeer createCanvas(Canvas target) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.CheckboxPeer createCheckbox(Checkbox target) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.CheckboxMenuItemPeer createCheckboxMenuItem(CheckboxMenuItem target) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.ChoicePeer createChoice(Choice target) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.DesktopPeer createDesktopPeer(Desktop target) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.DialogPeer createDialog(Dialog target) {
        throw new Error("Not implemented");
    }
    public java.awt.dnd.peer.DragSourceContextPeer createDragSourceContextPeer(java.awt.dnd.DragGestureEvent dge) {
        throw new Error("Not implemented");
    }
    public java.awt.peer.FileDialogPeer createFileDialog(FileDialog target) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.LabelPeer createLabel(Label target) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.ListPeer createList(List target) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.MenuPeer createMenu(Menu target) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.MenuBarPeer createMenuBar(MenuBar target) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.MenuItemPeer createMenuItem(MenuItem target) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.PanelPeer createPanel(Panel target) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.PopupMenuPeer createPopupMenu(PopupMenu target) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.ScrollbarPeer createScrollbar(Scrollbar target) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.ScrollPanePeer createScrollPane(ScrollPane target) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.TextAreaPeer createTextArea(TextArea target) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.TextFieldPeer createTextField(TextField target) {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.WindowPeer createWindow(Window target) {
        throw new Error("Not implemented");
    }
    public java.awt.datatransfer.Clipboard getSystemClipboard() {
        throw new Error("Not implemented");
    }
    public PrintJob getPrintJob(Frame frame, String jobtitle, java.util.Properties props)  {
        throw new Error("Not implemented");
    }
    protected java.awt.peer.FontPeer getFontPeer(String name, int style) {
        throw new Error("Not implemented");
    }
}
