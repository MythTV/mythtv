#ifndef MYTHPLAYEREDITORUI_H
#define MYTHPLAYEREDITORUI_H

// MythtTV
#include "mythplayervisualiserui.h"

class MythPlayerEditorUI : public MythPlayerVisualiserUI
{
    Q_OBJECT

  public:
    MythPlayerEditorUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);

    void    EnableEdit();
    void    DisableEdit(int HowToSave);
    bool    HandleProgramEditorActions(QStringList& Actions);
    uint64_t GetNearestMark(uint64_t Frame, bool Right);
    bool    IsTemporaryMark(uint64_t Frame);
    bool    HasTemporaryMark();
    bool    IsCutListSaved();
    bool    DeleteMapHasUndo();
    bool    DeleteMapHasRedo();
    QString DeleteMapGetUndoMessage();
    QString DeleteMapGetRedoMessage();
    void    HandleArbSeek(bool Direction);

  protected slots:
    void    InitialiseState() override;

  protected:
    bool    DoFastForwardSecs(float Seconds, double Inaccuracy, bool UseCutlist);
    bool    DoRewindSecs     (float Seconds, double Inaccuracy, bool UseCutlist);

    QElapsedTimer m_editUpdateTimer;
    float   m_speedBeforeEdit  { 1.0   };
    bool    m_pausedBeforeEdit { false };

  private:
    Q_DISABLE_COPY(MythPlayerEditorUI)
};

#endif
