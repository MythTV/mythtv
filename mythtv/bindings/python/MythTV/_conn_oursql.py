from logging import MythLog
from exceptions import MythDBError

import os
_allow_oursql = os.environ.get('ENABLE_OURSQL', False)
if _allow_oursql not in ['true', 'True', 'TRUE', 'yes', 'YES', '1']:
    raise ImportError('Refusing to import oursql')

import oursql

__version__ = tuple(['oursql']+oursql.__version__.split('.')) 

def dbconnect(dbconn, log):
    log(MythLog.DATABASE|MythLog.EXTRA,
                    'Spawning new database connection')
    db = oursql.connect(        dbconn['DBHostName'],
                                dbconn['DBUserName'],
                                dbconn['DBPassword'],
                        db=     dbconn['DBName'],
                        port=   dbconn['DBPort'],
                        use_unicode=True,
                        charset='utf8',
                        autoping=True,
                        raise_on_warnings=False)
    return db

class LoggedCursor( oursql.Cursor ):
    """
    Custom cursor, offering logging and error handling
    """
    log = None

    def log_query(self, query, args):
        self.log(self.log.DATABASE, ' '.join(query.split()), str(args))

    def _sanitize(self, query): return query.replace('%s', '?')

    def execute(self, query, args=None):
        """
        Execute a query.

        query -- string, query to execute on server
        args -- optional sequence, parameters to use with query.

        Returns long integer rows affected, if any
        """
        query = self._sanitize(query)
        self.log_query(query, args)
        try:
            if args:
                return super(LoggedCursor, self).execute(query, args)
            return super(LoggedCursor, self).execute(query)
        except Exception, e:
            raise MythDBError(MythDBError.DB_RAW, e.args)

    def executemany(self, query, args):
        """
        Execute a multi-row query.

        query -- string, query to execute on server
        args  -- Sequence of sequences, parameters to use with query.

        Returns long integer rows affected, if any.

        This method improves performance on multiple-row INSERT and
        REPLACE. Otherwise it is equivalent to looping over args with
        execute().
        """
        query = self._sanitize(query)
        self.log_query(query, args)
        try:
            return super(LoggedCursor, self).executemany(query, args)
        except Exception, e:
            raise MythDBError(MythDBError.DB_RAW, e.args)

    def commit(self): self.connection.commit()
    def rollback(self): self.connection.rollback()

