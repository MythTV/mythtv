#------------------------------
# MythTV/utility/dequebuffer.py
# Author: Raymond Wagner
# Description: A rolling buffer class that discards handled information.
#------------------------------

from io import BytesIO
from time import time, sleep
from threading import Thread, Lock
from collections import deque
from queue import Queue
import weakref

try:
    from select import poll, POLLHUP, POLLIN, POLLOUT
    class _PollingThread( Thread ):
        """
        This polling thread listens on selected pipes, and automatically reads
        and writes data between the buffer and those pipes. This will self
        terminate when there are no more pipes defined, and will need to be
        restarted.
        """
        def __init__(self, group=None, target=None, name=None,
                           args=(), kwargs={}):
            self.inputqueue = Queue()
            self.idletime = time()
            super(_PollingThread, self).__init__(group,
                        target, name, args, kwargs)
        def add_pipe(self, buff, pipe, mode):
            self.inputqueue.put((buff, pipe, mode))
        def run(self):
            poller = poll()
            fds = {}
            events = []
            while True:
                while not self.inputqueue.empty():
                    # loop though the queue and add new pipes to the
                    # poll object
                    buff, pipe, mode = self.inputqueue.get()
                    if 'r' in mode:
                        poller.register(pipe.fileno(), POLLIN|POLLHUP)
                    elif 'w' in mode:
                        poller.register(pipe.fileno(), POLLOUT|POLLHUP)
                    else:
                        continue
                    fds[pipe.fileno()] = (weakref.ref(buff), pipe)

                for fd,event in poller.poll(100):
                    # loop through file numbers and handle events
                    buff, pipe = fds[fd]
                    if buff() is None:
                        # buffer object has closed out from underneath us
                        # remove reference from poller
                        pipe.close()
                        del fds[fd]
                        poller.unregister(fd)
                        continue

                    if event & POLLIN:
                        # read as much data from the pipe as it has available
                        buff().write(pipe.read(2**16))
                    if event & POLLOUT:
                        # write as much data to the pipe as there is space for
                        # roll back buffer if data is not fully written
                        data = buff().read(2**16)
                        nbytes = pipe.write(data)
                        if nbytes != len(data):
                            buff()._rollback(len(data) - nbytes)
                    if event & POLLHUP:
                        # pipe has closed, and all reads have been processed
                        # remove reference from poller
                        buff().close()
                        pipe.close()
                        del fds[fd]
                        poller.unregister(fd)

                if len(fds) == 0:
                    # no pipes referenced
                    if self.idletime + 20 < time():
                        # idle timeout reached, terminate
                        break
                    sleep(0.1)
                else:
                    self.idletime = time()
except ImportError:
    from select import kqueue, kevent, KQ_FILTER_READ, KQ_FILTER_WRITE, \
                         KQ_EV_ADD, KQ_EV_DELETE, KQ_EV_EOF
    class _PollingThread( Thread ):
        """
        This polling thread listens on selected pipes, and automatically reads
        and writes data between the buffer and those pipes. This will self
        terminate when there are no more pipes defined, and will need to be
        restarted.
        """
        def __init__(self, group=None, target=None, name=None,
                           args=(), kwargs={}):
            self.inputqueue = Queue()
            self.idletime = time()
            super(_PollingThread, self).__init__(group,
                        target, name, args, kwargs)
        def add_pipe(self, buff, pipe, mode):
            self.inputqueue.put((buff, pipe, mode))
        def run(self):
            poller = kqueue()
            fds = {}
            events = []
            while True:
                while not self.inputqueue.empty():
                    # loop through the queue and gather new pipes to add the
                    # kernel queue
                    buff, pipe, mode = self.inputqueue.get()
                    if 'r' in mode:
                        events.append(kevent(pipe, KQ_FILTER_READ, KQ_EV_ADD))
                    elif 'w' in mode:
                        events.append(kevent(pipe, KQ_FILTER_WRITE, KQ_EV_ADD))
                    else:
                        continue
                    fds[pipe.fileno()] = (weakref.ref(buff), pipe)

                if len(events) == 0:
                    events = None
                events = poller.control(events, 16, 0.1)

                for i in range(len(events)):
                    # loop through response and handle events
                    event = events.pop()
                    buff, pipe = fds[event.ident]

                    if buff() is None:
                        # buffer object has closed out from underneath us
                        # pipe will be automatically removed from kqueue
                        pipe.close()
                        del fds[event.ident]
                        continue

                    if (abs(event.filter) & abs(KQ_FILTER_READ)) and event.data:
                        # new data has come in, push into the buffer
                        buff().write(pipe.read(event.data))

                    if (abs(event.filter) & abs(KQ_FILTER_WRITE)) and event.data:
                        # space is available to write data
                        pipe.write(buff().read(\
                                    min(buff()._nbytes, event.data, 2**16)))

                    if abs(event.flags) & abs(KQ_EV_EOF):
                        # pipe has been closed and all IO has been processed
                        # pipe will be automatically removed from kqueue
                        buff().close()
                        pipe.close()
                        del fds[event.ident]

                if len(fds) == 0:
                    # no pipes referenced
                    if self.idletime + 20 < time():
                        # idle timeout reached, terminate
                        break
                    sleep(0.1)
                else:
                    self.idletime = time()

class DequeBuffer( object ):
    """
    This is a chunked buffer, storing a sequence of buffer objects in a
    deque, allowing for FIFO operations outside the limited 64K system
    buffer, and the efficient freeing of memory without needing to rewrite
    a large contiguous buffer object.
    """
    class _Buffer( object ):
        """
        This subclass contains a buffer object and a read/write lock, as
        well as independent read and write positions.
        """
        __slots__ = ['buffer', 'lock', 'blocksize', 'EOF', 'rpos', 'wpos']
        def __init__(self, size=2**18):
            self.buffer = BytesIO()
            self.lock = Lock()
            self.blocksize = size
            self.EOF = False
            self.rpos = 0
            self.wpos = 0

        def __del__(self):
            self.buffer.close()

        def read(self, nbytes):
            with self.lock:
                self.buffer.seek(self.rpos)
                buff = self.buffer.read(nbytes)
                self.rpos = self.buffer.tell()
                if self.rpos == self.blocksize:
                    self.EOF = True
                return buff

        def write(self, data):
            with self.lock:
                nbytes = self.blocksize-self.wpos
                if nbytes < len(data):
                    data = data[:nbytes]
                else:
                    nbytes = len(data)
                self.buffer.seek(self.wpos)
                self.buffer.write(data)
                self.wpos += nbytes
            return nbytes

        def rollback(self, nbytes):
            with self.lock:
                self.EOF = False
                if self.rpos < nbytes:
                    nbytes -= self.rpos
                    self.rpos = 0
                    return nbytes
                else:
                    self.rpos -= nbytes
                    return 0

        def close(self):
            with self.lock:
                self.buffer.close()

    _pollingthread = None

    def __init__(self, data=None, inp=None, out=None):
        self._nbytes = 0
        self._buffer = deque()
        self._rollback_pool = []
        self._rpipe = None
        self._wpipe = None
        self._closed = False
        if data:
            self.write(data)
        if inp:
            self.attach_input(inp)
        if out:
            self.attach_output(out)

    def __len__(self):
        return self._nbytes

    def read(self, nbytes=None):
        """
        Read up to specified amount from buffer, or whatever is available.
        """
        # flush existing buffer
        self._rollback_pool = []
        data = BytesIO()
        while True:
            try:
                # get first item, or return if no more blocks are avaialable
                tmp = self._buffer[0]
            except IndexError:
                break

            if nbytes:
                # read only what is requested
                data.write(tmp.read(nbytes-data.tell()))
            else:
                # read all that is available
                data.write(tmp.read(tmp.blocksize))

            if tmp.EOF:
                # block is exhausted, cycle it into the rollback pool
                self._rollback_pool.append(self._buffer.popleft())
            else:
                # end of data or request reached, return
                break
        self._nbytes -= data.tell()
        return data.getvalue()

    def write(self, data):
        """Write provide data into buffer."""
        data = BytesIO(data)
        data.seek(0, 2)
        end = data.tell()
        pos = 0
        while True:
            data.seek(pos)
            try:
                # grab last entry in buffer
                tmp = self._buffer[-1]
            except IndexError:
                # buffer is empty, add a new block
                tmp = self._Buffer()
                self._buffer.append(tmp)
            # write as much data as possible, and update progress
            pos += tmp.write(data.read(tmp.blocksize))
            if pos == end:
                # no more data to write
                break
            else:
                # add another block, and loop back through
                self._buffer.append(self._Buffer())
        self._nbytes += end
        return end

    def _rollback(self, nbytes):
        """
        Roll back buffer specified number of bytes, pulling from a pool of
        expired blocks if needed. Pool will be flushed at the beginning of
        each read.
        """
        orig, nbytes = nbytes, self._buffer[0].rollback(nbytes)
        while nbytes:
            try:
                tmp = self._rollback_pool.pop()
            except IndexError:
                raise RuntimeError(('Tried to roll back {0} bytes into '+\
                        'DequeBuffer, but only {1} was available')\
                            .format(orig, orig-nbytes))
            else:
                self._buffer.appendleft(tmp)
                nbytes = tmp.rollback(nbytes)
        self._nbytes += nbytes

    def flush(self):
        pass

    def close(self):
        self._rpipe = None
        self._wpipe = None
        self._closed = True

    def attach_input(self, pipe):
        if self._rpipe is not None:
            raise RuntimeError('DequeBuffer is already connected to a '+\
                        'readable pipe')
        self._rpipe = pipe
        self._add_pipe(pipe, self, 'r')

    def attach_output(self, pipe):
        if self._wpipe is not None:
            raise RuntimeError('DequeBuffer is already connected to a '+\
                        'writable pipe')
        self._wpipe = pipe
        self._add_pipe(pipe, self, 'w')

    @classmethod
    def _add_pipe(cls, pipe, buffer, mode=None):
        """
        Add new pollable to the IO thread. Create new thread if necessary.
        Restart thread if it has terminated due to idle.
        """
        if not hasattr(pipe, 'fileno'):
            raise RuntimeError('DequeBuffer can only be attached to objects '+\
                    'with a fileno() method')
        if mode is None:
            # get IO mode from pipe
            mode = pipe.mode

        if (cls._pollingthread is None) or not cls._pollingthread.is_alive():
            # create new thread, and set it to not block shutdown
            cls._pollingthread = _PollingThread()
            cls._pollingthread.daemon = True
            cls._pollingthread.start()
        cls._pollingthread.add_pipe(buffer, pipe, mode)

