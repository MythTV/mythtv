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
import org.videolan.BDJXletContext;
import org.videolan.Logger;

public class HSceneFactory extends Object {
    private HSceneFactory() {
    }

    public static HSceneFactory getInstance() {
        BDJXletContext context = BDJXletContext.getCurrentContext();
        if (context != null) {
            if (context.getSceneFactory() == null) {
                context.setSceneFactory(new HSceneFactory());
            }
            return context.getSceneFactory();
        }

        logger.error("getInstance(): no context at " + Logger.dumpStack());

        return null;
    }

    public HSceneTemplate getBestSceneTemplate(HSceneTemplate template) {
        logger.unimplemented("getBestSceneTemplate");
        return null;
    }

    public HScene getBestScene(HSceneTemplate template) {
        /*
        if (defaultHScene != null) {
            logger.error("HScene already exists");
            return null;
        }
        */
        return getDefaultHScene();
        // TODO
    }

    public HSceneTemplate resizeScene(HScene scene, HSceneTemplate template)
            throws IllegalStateException
    {
        logger.unimplemented("resizeScene");
        return template;
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
        return getDefaultHScene(HScreen.getDefaultHScreen());
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

        if (scene == null) {
            logger.error("null HScene");
            return;
        }
        if (defaultHScene == null) {
            //logger.error("no HScene created");
            return;
        }

        if (!scene.equals(defaultHScene)) {
            logger.error("wrong HScene");
        }

        scene.disposeImpl();
        GUIManager.getInstance().remove(scene);
        defaultHScene = null;
    }

    public void dispose() {
        synchronized(HSceneFactory.class) {
            if (defaultHScene != null) {
                dispose(defaultHScene);
            }
        }
    }

    private HScene defaultHScene = null;

    private static final Logger logger = Logger.getLogger(HSceneFactory.class.getName());
}
