/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
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

package org.havi.ui;

import org.videolan.GUIManager;

public class HSceneFactory extends Object {
    private HSceneFactory() {
    }

    public static HSceneFactory getInstance() {
        return instance;
    }

    public HSceneTemplate getBestSceneTemplate(HSceneTemplate template) {
        throw new Error("Not implemented");
    }

    public HScene getBestScene(HSceneTemplate template) {
        return getDefaultHScene();
        // TODO
    }

    public HSceneTemplate resizeScene(HScene scene, HSceneTemplate template)
            throws IllegalStateException
    {
        throw new Error("Not implemented");
    }

    public HScene getDefaultHScene(HScreen screen)
    {
        synchronized(HSceneFactory.class) {
            if (defaultHScene == null) {
                defaultHScene = new HScene();
                defaultHScene.setLocation(0, 0);
                defaultHScene.setSize(GUIManager.getInstance().getWidth(), GUIManager.getInstance().getHeight());
                GUIManager.getInstance().add(defaultHScene);
            }
        }

        return defaultHScene;
    }

    public HScene getDefaultHScene()
    {
        synchronized(HSceneFactory.class) {
            if (defaultHScene == null) {
                defaultHScene = new HScene();
                defaultHScene.setLocation(0, 0);
                defaultHScene.setSize(GUIManager.getInstance().getWidth(), GUIManager.getInstance().getHeight());
                GUIManager.getInstance().add(defaultHScene);
            }
        }

        return defaultHScene;
    }

    public HScene getFullScreenScene(HGraphicsDevice device) {
        return getDefaultHScene();
/*
        HScene FullScreenScene = new HScene();
        FullScreenScene.setLocation(0, 0);
        FullScreenScene.setSize(GUIManager.getInstance().getWidth(), GUIManager.getInstance().getHeight());
        GUIManager.getInstance().add(FullScreenScene);
        return FullScreenScene;
*/
    }

    public void dispose(HScene scene) {
        GUIManager.getInstance().remove(scene);
    }

    private HScene defaultHScene = null;
    private static final HSceneFactory instance = new HSceneFactory();
}
