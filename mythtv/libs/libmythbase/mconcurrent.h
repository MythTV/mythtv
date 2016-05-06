#ifndef MCONCURRENT_H
#define MCONCURRENT_H

#include "mthreadpool.h"
#include "logging.h"


/// Provides a simple version of QtConcurrent::run() that uses MThreadPool rather
/// than QThreadPool. Useful for starting background threads in 1 line.
///
/// Given a class method of:
///
///   void Class::fn(arg1, arg2...)
///
/// you can run it in a different thread using:
///
///   MConcurrent::run("thread name", &Class instance, &Class::fn, arg1, arg2...)
///
/// Refer to QtConcurrent::run for further details
///
/// Restrictions:
/// 1. Accepts 0-5 arguments
/// 2. Only class methods are supported (most typical in Myth)
/// 3. Only non-const classes & methods are supported (most typical in Myth)
/// 4. The method must have return type of void (QFuture is not easily ported to
/// MThreadPool. Use signals/events instead)
///
namespace MConcurrent {

class RunFunctionTask : public QRunnable
{
public:
    void start(QString name)
    {
        MThreadPool::globalInstance()->start(this, name, /*m_priority*/ 0);
    }

    virtual void runFunctor() = 0;

    void run()
    {
        try
        {
            this->runFunctor();
        }
        catch (...)
        {
            LOG(VB_GENERAL, LOG_ERR, "An exception occurred");
        }
    }
};

template <typename Class>
class VoidStoredMemberFunctionPointerCall0 : public RunFunctionTask
{
public:
    VoidStoredMemberFunctionPointerCall0(void (Class::*_fn)() , Class *_object)
    : fn(_fn), object(_object) { }

    void runFunctor() { (object->*fn)(); }
private:
    void (Class::*fn)();
    Class *object;
};

template <typename Class, typename Param1, typename Arg1>
class VoidStoredMemberFunctionPointerCall1 : public RunFunctionTask
{
public:
    VoidStoredMemberFunctionPointerCall1(void (Class::*_fn)(Param1) , Class *_object, const Arg1 &_arg1)
    : fn(_fn), object(_object), arg1(_arg1){ }

    void runFunctor() { (object->*fn)(arg1); }
private:
    void (Class::*fn)(Param1);
    Class *object;
    Arg1 arg1;
};

template <typename Class, typename Param1, typename Arg1, typename Param2, typename Arg2>
class VoidStoredMemberFunctionPointerCall2 : public RunFunctionTask
{
public:
    VoidStoredMemberFunctionPointerCall2(void (Class::*_fn)(Param1, Param2) , Class *_object, const Arg1 &_arg1, const Arg2 &_arg2)
    : fn(_fn), object(_object), arg1(_arg1), arg2(_arg2){ }

    void runFunctor() { (object->*fn)(arg1, arg2); }
private:
    void (Class::*fn)(Param1, Param2);
    Class *object;
    Arg1 arg1; Arg2 arg2;
};

template <typename Class, typename Param1, typename Arg1, typename Param2, typename Arg2, typename Param3, typename Arg3>
class VoidStoredMemberFunctionPointerCall3 : public RunFunctionTask
{
public:
    VoidStoredMemberFunctionPointerCall3(void (Class::*_fn)(Param1, Param2, Param3) , Class *_object, const Arg1 &_arg1, const Arg2 &_arg2, const Arg3 &_arg3)
    : fn(_fn), object(_object), arg1(_arg1), arg2(_arg2), arg3(_arg3){ }

    void runFunctor() { (object->*fn)(arg1, arg2, arg3); }
private:
    void (Class::*fn)(Param1, Param2, Param3);
    Class *object;
    Arg1 arg1; Arg2 arg2; Arg3 arg3;
};

template <typename Class, typename Param1, typename Arg1, typename Param2, typename Arg2, typename Param3, typename Arg3, typename Param4, typename Arg4>
class VoidStoredMemberFunctionPointerCall4 : public RunFunctionTask
{
public:
    VoidStoredMemberFunctionPointerCall4(void (Class::*_fn)(Param1, Param2, Param3, Param4) , Class *_object, const Arg1 &_arg1, const Arg2 &_arg2, const Arg3 &_arg3, const Arg4 &_arg4)
    : fn(_fn), object(_object), arg1(_arg1), arg2(_arg2), arg3(_arg3), arg4(_arg4){ }

    void runFunctor() { (object->*fn)(arg1, arg2, arg3, arg4); }
private:
    void (Class::*fn)(Param1, Param2, Param3, Param4);
    Class *object;
    Arg1 arg1; Arg2 arg2; Arg3 arg3; Arg4 arg4;
};

template <typename Class, typename Param1, typename Arg1, typename Param2, typename Arg2, typename Param3, typename Arg3, typename Param4, typename Arg4, typename Param5, typename Arg5>
class VoidStoredMemberFunctionPointerCall5 : public RunFunctionTask
{
public:
    VoidStoredMemberFunctionPointerCall5(void (Class::*_fn)(Param1, Param2, Param3, Param4, Param5) , Class *_object, const Arg1 &_arg1, const Arg2 &_arg2, const Arg3 &_arg3, const Arg4 &_arg4, const Arg5 &_arg5)
    : fn(_fn), object(_object), arg1(_arg1), arg2(_arg2), arg3(_arg3), arg4(_arg4), arg5(_arg5){ }

    void runFunctor() { (object->*fn)(arg1, arg2, arg3, arg4, arg5); }
private:
    void (Class::*fn)(Param1, Param2, Param3, Param4, Param5);
    Class *object;
    Arg1 arg1; Arg2 arg2; Arg3 arg3; Arg4 arg4; Arg5 arg5;
};

template <typename Class>
void run(const QString &name, Class *object, void (Class::*fn)())
{
    (new VoidStoredMemberFunctionPointerCall0<Class>(fn, object))->start(name);
}

template <typename Class, typename Param1, typename Arg1>
void run(const QString &name, Class *object, void (Class::*fn)(Param1), const Arg1 &arg1)
{
    (new VoidStoredMemberFunctionPointerCall1<Class, Param1, Arg1>(fn, object, arg1))->start(name);
}

template <typename Class, typename Param1, typename Arg1, typename Param2, typename Arg2>
void run(const QString &name, Class *object, void (Class::*fn)(Param1, Param2), const Arg1 &arg1, const Arg2 &arg2)
{
    (new VoidStoredMemberFunctionPointerCall2<Class, Param1, Arg1, Param2, Arg2>(fn, object, arg1, arg2))->start(name);
}

template <typename Class, typename Param1, typename Arg1, typename Param2, typename Arg2, typename Param3, typename Arg3>
void run(const QString &name, Class *object, void (Class::*fn)(Param1, Param2, Param3), const Arg1 &arg1, const Arg2 &arg2, const Arg3 &arg3)
{
    (new VoidStoredMemberFunctionPointerCall3<Class, Param1, Arg1, Param2, Arg2, Param3, Arg3>(fn, object, arg1, arg2, arg3))->start(name);
}

template <typename Class, typename Param1, typename Arg1, typename Param2, typename Arg2, typename Param3, typename Arg3, typename Param4, typename Arg4>
void run(const QString &name, Class *object, void (Class::*fn)(Param1, Param2, Param3, Param4), const Arg1 &arg1, const Arg2 &arg2, const Arg3 &arg3, const Arg4 &arg4)
{
    (new VoidStoredMemberFunctionPointerCall4<Class, Param1, Arg1, Param2, Arg2, Param3, Arg3, Param4, Arg4>(fn, object, arg1, arg2, arg3, arg4))->start(name);
}

template <typename Class, typename Param1, typename Arg1, typename Param2, typename Arg2, typename Param3, typename Arg3, typename Param4, typename Arg4, typename Param5, typename Arg5>
void run(const QString &name, Class *object, void (Class::*fn)(Param1, Param2, Param3, Param4, Param5), const Arg1 &arg1, const Arg2 &arg2, const Arg3 &arg3, const Arg4 &arg4, const Arg4 &arg5)
{
    (new VoidStoredMemberFunctionPointerCall5<Class, Param1, Arg1, Param2, Arg2, Param3, Arg3, Param4, Arg4, Param5, Arg5>(fn, object, arg1, arg2, arg3, arg4, arg5))->start(name);
}


} //namespace QtConcurrent

#endif // MCONCURRENT_H

