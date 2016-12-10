/*
 * This file is part of libbluray
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

package org.videolan.media.content;

import java.util.ArrayList;
import org.videolan.Logger;

public class PlayerManager {

    private static PlayerManager instance = new PlayerManager();

    public static PlayerManager getInstance() {
        return instance;
    }

    /*
     *
     */

    private ArrayList registeredPlayers = new ArrayList(1);

    private BDHandler playlistPlayer = null;
    private BDHandler videoDripPlayer = null;
    //private ArrayList audioPlayerList = new ArrayList(8);

    private Object playlistPlayerLock = new Object();
    private Object videoDripPlayerLock = new Object();
    //private Object audioPlayerLock = new Object();
    private Object  stoppingLock = new Object();
    private boolean stopping = false;

    public void releaseAllPlayers(boolean unconditional) {
        BDHandler[] players = null;
        synchronized (registeredPlayers) {
            players = (BDHandler[])registeredPlayers.toArray(new BDHandler[0]);
        }

        for (int i = 0; i < players.length; i++) {
            if (unconditional) {
                players[i].close();
            } else if (players[i].getOwnerContext() != null && players[i].getOwnerContext().isReleased()) {
                players[i].close();
            }
        }
    }

    public BDHandler getPlaylistPlayer() {
        synchronized (playlistPlayerLock) {
            return playlistPlayer;
        }
    }

    protected void releaseResource(BDHandler player) {
        if (player instanceof org.videolan.media.content.playlist.Handler) {
            synchronized (playlistPlayerLock) {
                if (player == playlistPlayer) {
                    playlistPlayer = null;
                }
            }
            return;
        }
        if (player instanceof org.videolan.media.content.sound.Handler) {
            return;
        }
        if (player instanceof org.videolan.media.content.audio.Handler) {
            return;
        }

        logger.error("unknown player type: " + player.getClass().getName());
    }

    protected boolean allocateResource(BDHandler player) {
        if (player instanceof org.videolan.media.content.playlist.Handler) {
            synchronized (stoppingLock) {
                stopping = true;
            }
            synchronized (playlistPlayerLock) {
                if (playlistPlayer != null && player != playlistPlayer) {

                    logger.info("allocateResource(): Stopping old playlist player");

                    playlistPlayer.stop();
                    playlistPlayer.deallocate();
                }
                playlistPlayer = player;
            }
            synchronized (stoppingLock) {
                stopping = false;
            }
            return true;
        }
        if (player instanceof org.videolan.media.content.sound.Handler) {
            return true;
        }
        if (player instanceof org.videolan.media.content.audio.Handler) {
            return true;
        }

        logger.error("allocateResource(): unknown player type: " + player.getClass().getName());
        return false;
    }

    protected void unregisterPlayer(BDHandler player)
    {
        synchronized (registeredPlayers) {
            if (registeredPlayers.contains(player)) {
                registeredPlayers.remove(player);
            }
        }
    }

    protected void registerPlayer(BDHandler player)
    {
        synchronized (registeredPlayers) {
            if (!registeredPlayers.contains(player)) {
                registeredPlayers.add(player);
            }
        }
    }

    /*
     *
     */

    public boolean onEvent(int event, int param) {
        synchronized (stoppingLock) {
            if (stopping) return false;
            synchronized (playlistPlayerLock) {
                if (playlistPlayer != null)
                    return playlistPlayer.statusEvent(event, param);
            }
        }
        return false;
    }

    private static final Logger logger = Logger.getLogger(PlayerManager.class.getName());
}
