#ifndef MYTHPLAYEREDITORUI_H
#define MYTHPLAYEREDITORUI_H

// MythtTV
#include "mythplayervisualiserui.h"

class MythPlayerEditorUI : public MythPlayerVisualiserUI
{
    Q_OBJECT

  signals:
    void EditorStateChanged(const MythEditorState& EditorState);

  public:
    MythPlayerEditorUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);

    bool    HandleProgramEditorActions(const QStringList& Actions);

  protected slots:
    void    InitialiseState() override;
    void    RefreshEditorState(bool CheckSaved = false);
    void    EnableEdit();
    void    DisableEdit(int HowToSave);

  protected:
    void    HandleArbSeek(bool Direction);
    bool    DoFastForwardSecs(float Seconds, double Inaccuracy, bool UseCutlist);
    bool    DoRewindSecs     (float Seconds, double Inaccuracy, bool UseCutlist);

    QElapsedTimer m_editUpdateTimer;
    float   m_speedBeforeEdit  { 1.0   };
    bool    m_pausedBeforeEdit { false };

  private:
    Q_DISABLE_COPY(MythPlayerEditorUI)
};

#endif
