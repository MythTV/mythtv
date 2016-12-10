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

import java.awt.Component;
import java.io.IOException;

import javax.media.Clock;
import javax.media.ClockStoppedException;
import javax.media.Control;
import javax.media.Controller;
import javax.media.ControllerClosedEvent;
import javax.media.ControllerErrorEvent;
import javax.media.ControllerEvent;
import javax.media.ControllerListener;
import javax.media.DeallocateEvent;
import javax.media.EndOfMediaEvent;
import javax.media.GainControl;
import javax.media.IncompatibleSourceException;
import javax.media.IncompatibleTimeBaseException;
import javax.media.Manager;
import javax.media.NotPrefetchedError;
import javax.media.NotRealizedError;
import javax.media.Player;
import javax.media.PrefetchCompleteEvent;
import javax.media.RateChangeEvent;
import javax.media.RealizeCompleteEvent;
import javax.media.ResourceUnavailableEvent;
import javax.media.StartEvent;
import javax.media.StopByRequestEvent;
import javax.media.StopTimeChangeEvent;
import javax.media.Time;
import javax.media.TimeBase;
import javax.media.TransitionEvent;
import javax.media.protocol.DataSource;

import javax.tv.locator.Locator;
import javax.tv.service.selection.ServiceContentHandler;

import org.davic.media.MediaTimePositionChangedEvent;

import org.bluray.media.OverallGainControl;

import org.videolan.BDJAction;
import org.videolan.BDJActionManager;
import org.videolan.BDJActionQueue;
import org.videolan.BDJListeners;
import org.videolan.BDJXletContext;
import org.videolan.Libbluray;
import org.videolan.Logger;

public abstract class BDHandler implements Player, ServiceContentHandler {

    public BDHandler() {
        ownerContext = BDJXletContext.getCurrentContext();
        if (ownerContext == null) {
            doInitAction();
        } else {
            PlayerAction action = new PlayerAction(this, PlayerAction.ACTION_INIT, null);
            BDJActionManager.getInstance().putCommand(action);
            action.waitEnd();
        }
    }

    private void doInitAction() {
        commandQueue = new BDJActionQueue("MediaPlayer");
        PlayerManager.getInstance().registerPlayer(this);
    }

    protected BDJXletContext getOwnerContext() {
        return ownerContext;
    }

    private void checkUnrealized() {
        synchronized (this) {
            if (state == Unrealized)
                throw new NotRealizedError("Player Unrealized");
        }
    }

    public abstract void setSource(DataSource source)
                throws IOException, IncompatibleSourceException;

    public int getState() {
        synchronized (this) {
            return state;
        }
    }

    public int getTargetState() {
        Logger.unimplemented("BDHandler", "getTargetState()");
        synchronized (this) {
            return targetState;
        }
    }

    public Control[] getControls() {
        return controls;
    }

    public Control getControl(String forName) {
        try {
            Class cls = Class.forName(forName);
            for (int i = 0; i < controls.length; i++) {
                if (cls.isInstance(controls[i]))
                    return controls[i];
            }
            Logger.getLogger("BDHandler").error("getControl(): control not found: " + forName);
            return null;
        } catch (ClassNotFoundException e) {
            Logger.getLogger("BDHandler").error("getControl(): " + e);
            return null;
        }
    }

    public GainControl getGainControl() {
        checkUnrealized();
        for (int i = 0; i < controls.length; i++) {
            if (controls[i] instanceof OverallGainControl)
                return (GainControl)controls[i];
        }
        return null;
    }

    public void addController(Controller newController) throws IncompatibleTimeBaseException {
        checkUnrealized();
    }

    public void removeController(Controller oldController) {
        checkUnrealized();
    }

    public Component getControlPanelComponent() {
        checkUnrealized();
        return null;
    }

    public Component getVisualComponent() {
        checkUnrealized();
        return null;
    }

    public void addControllerListener(ControllerListener listener) {
        listeners.add(listener);
    }

    public void removeControllerListener(ControllerListener listener) {
        listeners.remove(listener);
    }

    public Time getStartLatency() {
        checkUnrealized();
        return new Time(0.1d);
    }

    public TimeBase getTimeBase() {
        checkUnrealized();
        return Manager.getSystemTimeBase();
    }

    public void setTimeBase(TimeBase master) throws IncompatibleTimeBaseException {
        checkUnrealized();
        throw new IncompatibleTimeBaseException();
    }

    public Time getStopTime() {
        return stopTime;
    }

    public void setStopTime(Time stopTime) {
        checkUnrealized();
        // TODO: actually stopping when stop time is hit needs to be implemented
        this.stopTime = stopTime;

        postStopTimeChangeEvent();
    }

    public Time getMediaTime() {
        synchronized (this) {
            if (state == Started)
                return new Time((baseMediaTime + (getTimeBase().getNanoseconds() - baseTime)) / 1000000000.0d);
            else
                return new Time(baseMediaTime / 1000000000.0d);
        }
    }

    public long getMediaNanoseconds() {
        synchronized (this) {
            if ((state == Started) && (rate != 0.0f))
                return baseMediaTime + (getTimeBase().getNanoseconds() - baseTime);
            else
                return baseMediaTime;
        }
    }

    public Time getSyncTime() {
        return getMediaTime();
    }

    public void setMediaTime(Time now) {
        checkUnrealized();

        if (isClosed) return;

        PlayerAction action = new PlayerAction(this, PlayerAction.ACTION_SEEK_TIME, now);
        commandQueue.put(action);
        action.waitEnd();
    }

    public void setMediaTimePosition(Time mediaTime) {
        setMediaTime(mediaTime);
        postMediaTimePositionChangedEvent();
    }

    public Time mapToTimeBase(Time t) throws ClockStoppedException {
        if (state != Started)
            throw new ClockStoppedException();
        return getMediaTime();
    }

    public abstract Time getDuration();

    protected void updateTime(Time now) {
        baseMediaTime = now.getNanoseconds();
        baseTime = getTimeBase().getNanoseconds();
    }

    public float getRate() {
        return rate;
    }

    public float setRate(float factor) {
        checkUnrealized();

        PlayerAction action = new PlayerAction(this, PlayerAction.ACTION_SET_RATE, new Float(factor));
        commandQueue.put(action);
        action.waitEnd();
        return rate;
    }

    public static final int GAIN_OVERALL = 1;
    public static final int GAIN_PRIMARY = 2;
    public static final int GAIN_SECONDARY = 3;

    public void setGain(int mixer, boolean mute, float level) {
        Logger.unimplemented("BDHandler", "setGain");
    }

    public void setPanning(float x, float y) {
        Logger.unimplemented("BDHandler", "setPanning");
    }

    public Locator[] getServiceContentLocators() {
        return new Locator[0];
    }

    public void realize() {
        if (isClosed) return;

        PlayerAction action = new PlayerAction(this, PlayerAction.ACTION_REALIZE, null);
        commandQueue.put(action);
    }

    public void prefetch() {
        if (isClosed) return;

        PlayerAction action = new PlayerAction(this, PlayerAction.ACTION_PREFETCH, null);
        commandQueue.put(action);
    }

    public void syncStart(Time at) {
        synchronized (this) {
            if (state != Prefetched)
                throw new NotPrefetchedError("syncStart");
        }
        if (isClosed) return;

        PlayerAction action = new PlayerAction(this, PlayerAction.ACTION_START, at);
        commandQueue.put(action);
    }

    public void start() {
        if (isClosed) return;

        PlayerAction action = new PlayerAction(this, PlayerAction.ACTION_START, null);
        commandQueue.put(action);
    }

    public void stop() {
        if (isClosed) return;

        PlayerAction action = new PlayerAction(this, PlayerAction.ACTION_STOP, null);
        commandQueue.put(action);
        action.waitEnd();
    }

    public void deallocate() {
        if (isClosed) return;

        if (state == Started) {
        }
        PlayerAction action = new PlayerAction(this, PlayerAction.ACTION_DEALLOCATE, null);
        commandQueue.put(action);
        action.waitEnd();

        PlayerManager.getInstance().releaseResource(this);
    }

    public void close() {
        if (isClosed) return;

        stop();
        deallocate();

        PlayerAction action = new PlayerAction(this, PlayerAction.ACTION_CLOSE, null);
        commandQueue.put(action);
        action.waitEnd();

        isClosed = true;
        commandQueue.shutdown();
    }

    /*
     * notifications from app
     */

    protected boolean statusEvent(int event, int param) {
        if (isClosed) return false;
        commandQueue.put(new PlayerAction(this, PlayerAction.ACTION_STATUS, new Integer(event), param));
        return true;
    }

    /*
     * handling of notifications from app
     */

    /* handle notification from app */
    protected void doRateChanged(float rate) {
        if (this.rate != rate) {
            this.rate = rate;
            notifyListeners(new RateChangeEvent(this, rate));
        }
    }

    protected void doTimeChanged(int time) {
        //System.out.println("current time = " + time * TO_SECONDS);
        //currentTime = new Time(time * TO_SECONDS);
        //
        // TODO - not handled !
    }

    protected void doEndOfMediaReached(int playlist) {
        if (state == Started) {
            ControllerErrorEvent error = doStop(true);
            if (error == null) {
                state = Prefetched;
                notifyListeners(new EndOfMediaEvent(this, Started, Prefetched, Prefetched, getMediaTime()));
            } else {
                notifyListeners(error);
            }
        }
    }

    protected void doSeekNotify(long tick) {
        updateTime(new Time(tick * TO_SECONDS));
    }

    protected void doPlaylistStarted(int playlist) {};
    protected void doChapterReached(int chapter) {};
    protected void doMarkReached(int playmark) {};
    protected void doPlayItemReached(int playitem) {};
    protected void doAngleChanged(int angle) {};
    protected void doSubtitleChanged(int param) {};
    protected void doAudioStreamChanged(int param) {};
    protected void doUOMasked(int position) {};
    protected void doSecondaryStreamChanged(int param) {};

    /*
     *
     */

    protected ControllerErrorEvent doRealize() {
        return null;
    }

    protected ControllerErrorEvent doPrefetch() {
        return null;
    }

    protected ControllerErrorEvent doStart(Time at) {
        if (at != null)
            baseMediaTime = at.getNanoseconds();
        baseTime = getTimeBase().getNanoseconds();
        return null;
    }

    protected ControllerErrorEvent doStop(boolean eof) {
        baseMediaTime = getMediaNanoseconds();
        rate = 1.0f;
        return null;
    }

    protected ControllerErrorEvent doDeallocate() {
        return null;
    }

    protected ControllerErrorEvent doClose() {
        return null;
    }

    protected void doSeekTime(Time at) {
        updateTime(at);
    }

    protected void doSetRate(Float factor) {
        if (rate != factor.floatValue()) {
            rate = factor.floatValue();
            notifyListeners(new RateChangeEvent(this, rate));
        }
    }

    /*
     *
     */

    private void postStopTimeChangeEvent() {
        notifyListeners(new StopTimeChangeEvent(this, getStopTime()));
    }

    protected void postMediaTimePositionChangedEvent() {
        notifyListeners(new MediaTimePositionChangedEvent(this, getState(), getState(), getState(), getMediaTime()));
    }

    private void notifyListeners(ControllerEvent event) {
        listeners.putCallback(event);
    }

    private boolean doRealizeAction() {
        switch (state) {
        case Unrealized:
            state = Realizing;
            notifyListeners(new TransitionEvent(this, Unrealized, Realizing, Realized));
            /* fall thru */
        case Realizing:
            ControllerErrorEvent error = doRealize();
            if (error == null) {
                state = Realized;
                notifyListeners(new RealizeCompleteEvent(this, Realizing, Realized, Realized));
            } else {
                notifyListeners(error);
                return false;
            }
            break;
        default:
            notifyListeners(new RealizeCompleteEvent(this, state, state, state));
            break;
        }
        return true;
    }

    private boolean doPrefetchAction() {
        switch (state) {
        case Unrealized:
        case Realizing:
            if (!doRealizeAction())
                return false;
            /* fall thru */
        case Realized:
            state = Prefetching;
            notifyListeners(new TransitionEvent(this, Realized, Prefetching, Prefetched));
            /* fall thru */
        case Prefetching:

            if (!PlayerManager.getInstance().allocateResource(this)) {
                notifyListeners(new ResourceUnavailableEvent(this));
                return false;
            }
            ControllerErrorEvent error = doPrefetch();
            if (error == null) {
                state = Prefetched;
                notifyListeners(new PrefetchCompleteEvent(this, Prefetching, Prefetched, Prefetched));
            } else {
                notifyListeners(error);
                return false;
            }
            break;
        default:
            notifyListeners(new PrefetchCompleteEvent(this, state, state, state));
            break;
        }
        return true;
    }

    private boolean doStartAction(Time at) {
        switch (state) {
        case Unrealized:
        case Realizing:
            if (!doRealizeAction())
                return false;
            /* fall thru */
        case Realized:
        case Prefetching:
            if (!doPrefetchAction())
                return false;
            /* fall thru */
        case Prefetched:
            ControllerErrorEvent error = doStart(at);
            if (error == null) {
                state = Started;
                notifyListeners(new StartEvent(this, Prefetched, Started, Started, at, getMediaTime()));
            } else {
                notifyListeners(error);
                return false;
            }
            break;
        default:
            notifyListeners(new StartEvent(this, state, state, state, at, getMediaTime()));
            break;
        }
        return true;
    }

    private boolean doStopAction() {
        switch (state) {
        case Started:
            ControllerErrorEvent error = doStop(false);
            if (error == null) {
                state = Prefetched;
                notifyListeners(new StopByRequestEvent(this, Started, Prefetched, Prefetched, getMediaTime()));
            } else {
                notifyListeners(error);
                return false;
            }
            break;
        default:
            notifyListeners(new StopByRequestEvent(this, state, state, state, getMediaTime()));
            break;
        }
        return true;
    }

    private boolean doDeallocateAction() {
        ControllerErrorEvent error;
        switch (state) {
        case Realizing:
            error = doDeallocate();
            if (error == null) {
                state = Unrealized;
                notifyListeners(new DeallocateEvent(this, Realizing, Unrealized, Unrealized, getMediaTime()));
            } else {
                notifyListeners(error);
                return false;
            }
            break;
        case Unrealized:
        case Realized:
            notifyListeners(new DeallocateEvent(this, state, state, state, getMediaTime()));
            break;
        default:
            error = doDeallocate();
            if (error == null) {
                int previous = state;
                state = Realized;
                notifyListeners(new DeallocateEvent(this, previous, Realized, Realized, getMediaTime()));
            } else {
                notifyListeners(error);
                return false;
            }
            break;
        }
        return true;
    }

    private void doCloseAction() {
        doClose();
        notifyListeners(new ControllerClosedEvent(this));
        PlayerManager.getInstance().unregisterPlayer(this);
    }

    private class PlayerAction extends BDJAction {
        private PlayerAction(BDHandler player, int action, Object param) {
            this(player, action, param, -1);
        }
        private PlayerAction(BDHandler player, int action, Object param, int param2) {
            this.player = player;
            this.action = action;
            this.param = param;
            this.param2 = param2;
        }

        protected void doAction() {
            switch (action) {
            case ACTION_INIT:
                player.doInitAction();
                break;
            case ACTION_REALIZE:
                player.doRealizeAction();
                break;
            case ACTION_PREFETCH:
                player.doPrefetchAction();
                break;
            case ACTION_START:
                player.doStartAction((param == null) ? null : (Time)param);
                break;
            case ACTION_STOP:
                player.doStopAction();
                break;
            case ACTION_DEALLOCATE:
                player.doDeallocateAction();
                break;
            case ACTION_CLOSE:
                player.doCloseAction();
                break;

            case ACTION_SEEK_TIME:
                player.doSeekTime((Time)param);
                break;
            case ACTION_SET_RATE:
                player.doSetRate((Float)param);
                break;

            case ACTION_STATUS:
                switch (((Integer)param).intValue()) {
                case Libbluray.BDJ_EVENT_CHAPTER:
                    player.doChapterReached(param2);
                    break;
                case Libbluray.BDJ_EVENT_MARK:
                    player.doMarkReached(param2);
                    break;
                case Libbluray.BDJ_EVENT_PLAYITEM:
                    player.doPlayItemReached(param2);
                    break;
                case Libbluray.BDJ_EVENT_PLAYLIST:
                    player.doPlaylistStarted(param2);
                    break;
                case Libbluray.BDJ_EVENT_ANGLE:
                    player.doAngleChanged(param2);
                    break;
                case Libbluray.BDJ_EVENT_SUBTITLE:
                    player.doSubtitleChanged(param2);
                    break;
                case Libbluray.BDJ_EVENT_END_OF_PLAYLIST:
                    player.doEndOfMediaReached(param2);
                    break;
                case Libbluray.BDJ_EVENT_PTS:
                    player.doTimeChanged(param2);
                    break;
                case Libbluray.BDJ_EVENT_AUDIO_STREAM:
                    player.doAudioStreamChanged(param2);
                    break;
                case Libbluray.BDJ_EVENT_SECONDARY_STREAM:
                    player.doSecondaryStreamChanged(param2);
                    break;
                case Libbluray.BDJ_EVENT_UO_MASKED:
                    player.doUOMasked(param2);
                    break;
                case Libbluray.BDJ_EVENT_SEEK:
                    player.doSeekNotify(param2 * 2 /* 45kHz -> 90kHz */);
                    break;
                case Libbluray.BDJ_EVENT_RATE:
                    float rate = (float)param2 / 90000.0f;
                    if (rate < 0.0f) rate = -rate;
                    if (rate < 0.01f) rate = 0.0f;
                    if (rate > 0.99f && rate < 1.01f) rate = 1.0f;
                    player.doRateChanged(rate);
                    break;
                default:
                    System.err.println("Unknown ACTION_STATUS: id " + param + ", value " + param2);
                    break;
                }
                break;
            default:
                System.err.println("Unknown action " + action);
                break;
            }
        }

        private BDHandler player;
        private int action;
        private Object param;
        private int param2;

        public static final int ACTION_INIT = 1;
        public static final int ACTION_REALIZE = 2;
        public static final int ACTION_PREFETCH = 3;
        public static final int ACTION_START = 4;
        public static final int ACTION_STOP = 5;
        public static final int ACTION_DEALLOCATE = 6;
        public static final int ACTION_CLOSE = 7;

        public static final int ACTION_SEEK_TIME = 8;
        public static final int ACTION_SET_RATE = 9;

        public static final int ACTION_STATUS = 10;
    }

    protected int state = Unrealized;
    protected int targetState = Unrealized;
    protected Time stopTime = Clock.RESET;
    protected long baseMediaTime = 0;
    protected long baseTime;
    protected float rate = 1.0f;
    protected Control[] controls = null;
    private BDJListeners listeners = new BDJListeners();
    private BDJXletContext ownerContext;
    boolean isClosed = false;

    protected BDJActionQueue commandQueue;

    public static final double TO_SECONDS = 1 / 90000.0d;
    public static final double FROM_SECONDS = 90000.0d;
    public static final double TO_NAROSECONDS = 1000000 / 90.0d;
    public static final double FROM_NAROSECONDS = 0.00009d;
}
