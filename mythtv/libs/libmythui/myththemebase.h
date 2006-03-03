#ifndef MYTHTHEMEBASE_H_
#define MYTHTHEMEBASE_H_

class MythThemeBasePrivate;

class MythThemeBase
{
  public:
    MythThemeBase();
   ~MythThemeBase();

    void Reload(void);

  private:
    void Init(void);

    MythThemeBasePrivate *d;
};

#endif

