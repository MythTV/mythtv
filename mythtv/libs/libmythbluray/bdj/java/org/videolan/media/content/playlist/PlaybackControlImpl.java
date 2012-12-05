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

package org.videolan.media.content.playlist;

import java.awt.Component;
import java.util.LinkedList;

import org.bluray.media.PlaybackControl;
import org.bluray.media.PlaybackListener;
import org.bluray.media.PlaybackMarkEvent;
import org.bluray.media.PlaybackPlayItemEvent;
import org.videolan.BDJAction;
import org.videolan.BDJActionManager;
import org.videolan.PlaylistInfo;
import org.videolan.TIMark;

public class PlaybackControlImpl implements PlaybackControl {
    protected PlaybackControlImpl(Handler player) {
        this.player = player;
    }

    public Component getControlComponent() {
        return null;
    }

    public void addPlaybackControlListener(PlaybackListener listener) {
        synchronized(listeners) {
            listeners.add(listener);
        }
    }

    public void removePlaybackControlListener(PlaybackListener listener) {
        synchronized(listeners) {
            listeners.remove(listener);
        }
    }

    public void skipToMark(int mark) throws IllegalArgumentException {
        player.seekMark(mark);
    }

    public boolean skipToNextMark(int type) throws IllegalArgumentException {
        if ((type != TIMark.MARK_TYPE_ENTRY) && (type != TIMark.MARK_TYPE_LINK))
            throw new IllegalArgumentException();
        PlaylistInfo pi = player.getPlaylistInfo();
        if (pi == null)
            return false;
        TIMark[] marks = pi.getMarks();
        if (marks == null)
            return false;
        long time = player.getMediaNanoseconds();
        for (int i = 0; i < marks.length; i++) {
            if ((marks[i].getType() == type) &&
                (marks[i].getStartNanoseconds() > time)) {
                player.seekMark(i);
                return true;
            }
        }
        return false;
    }

    public boolean skipToPreviousMark(int type) throws IllegalArgumentException {
        if ((type != TIMark.MARK_TYPE_ENTRY) && (type != TIMark.MARK_TYPE_LINK))
            throw new IllegalArgumentException();
        PlaylistInfo pi = player.getPlaylistInfo();
        if (pi == null)
            return false;
        TIMark[] marks = pi.getMarks();
        if (marks == null)
            return false;
        long time = player.getMediaNanoseconds();
        for (int i = marks.length - 1; i >= 0; i--) {
            if ((marks[i].getType() == type) &&
                (marks[i].getStartNanoseconds() < time)) {
                player.seekMark(i);
                return true;
            }
        }
        return false;
    }

    public void skipToPlayItem(int item) throws IllegalArgumentException {
        player.seekPlayItem(item);
    }

    protected void onChapterReach(int chapter) {
        if (chapter <= 0)
            return;
        chapter--;
        PlaylistInfo pi = player.getPlaylistInfo();
        if (pi == null)
            return;
        TIMark[] marks = pi.getMarks();
        if (marks == null)
            return;
        for (int i = 0, j = 0; i < marks.length; i++) {
            if (marks[i].getType() == TIMark.MARK_TYPE_ENTRY) {
                if (j == chapter) {
                    notifyListeners(new PlaybackMarkEvent(this, i));
                    return;
                }
                j++;
            }
        }
    }

    protected void onPlayItemReach(int param) {
        notifyListeners(new PlaybackPlayItemEvent(this, param));
    }

    private void notifyListeners(Object event) {
        synchronized (listeners) {
            if (!listeners.isEmpty())
                BDJActionManager.getInstance().putCallback(
                        new PlayeBackCallback(this, event));
        }
    }

    private class PlayeBackCallback extends BDJAction {
        private PlayeBackCallback(PlaybackControlImpl control, Object event) {
            this.control = control;
            this.event = event;
        }

        protected void doAction() {
            LinkedList list;
            synchronized (control.listeners) {
                list = (LinkedList)control.listeners.clone();
            }
            if (event instanceof PlaybackMarkEvent) {
                for (int i = 0; i < list.size(); i++)
                    ((PlaybackListener)list.get(i)).markReached((PlaybackMarkEvent)event);
            } else if (event instanceof PlaybackPlayItemEvent) {
                for (int i = 0; i < list.size(); i++)
                    ((PlaybackListener)list.get(i)).playItemReached((PlaybackPlayItemEvent)event);
            }
        }

        private PlaybackControlImpl control;
        private Object event;
    }

    private LinkedList listeners = new LinkedList();
    private Handler player;
}
