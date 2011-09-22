/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

#ifndef _PANE_ATSC_H_
#define _PANE_ATSC_H_

#include <algorithm>
using namespace std;

// MythTV headers
#include "channelscanmiscsettings.h"
#include "frequencytablesetting.h"
#include "modulationsetting.h"
#include "frequencytables.h"

class PaneATSC : public VerticalConfigurationGroup
{
    Q_OBJECT

  public:
    PaneATSC() :
        VerticalConfigurationGroup(false, false, true, false),
        atsc_table(new ScanFrequencyTable()),
        atsc_modulation(new ScanATSCModulation())
    {
        addChild(atsc_table);
        addChild(atsc_modulation);

        connect(atsc_table, SIGNAL(valueChanged(    const QString&)),
                this,       SLOT(  FreqTableChanged(const QString&)));

        connect(atsc_modulation, SIGNAL(valueChanged(     const QString&)),
                this,            SLOT(  ModulationChanged(const QString&)));

        HorizontalConfigurationGroup *range =
            new HorizontalConfigurationGroup(false,false,true,true);
        transport_start = new TransComboBoxSetting();
        transport_end   = new TransComboBoxSetting();
        transport_count = new TransLabelSetting();
        TransLabelSetting *label = new TransLabelSetting();
        label->setLabel(tr("Scanning Range"));
        range->addChild(label);
        range->addChild(transport_start);
        range->addChild(transport_end);
        range->addChild(transport_count);
        addChild(range);

        connect(transport_start, SIGNAL(valueChanged(         const QString&)),
                this,            SLOT(  TransportRangeChanged(const QString&)));
        connect(transport_end,   SIGNAL(valueChanged(         const QString&)),
                this,            SLOT(  TransportRangeChanged(const QString&)));

        ResetTransportRange();
    }

    ~PaneATSC()
    {
        while (!tables.empty())
        {
            delete tables.back();
            tables.pop_back();
        }
    }

    QString GetFrequencyTable(void) const
        { return atsc_table->getValue(); }
    QString GetModulation(void) const
        { return atsc_modulation->getValue(); }
    bool GetFrequencyTableRange(
        QString &start, QString &end) const
    {
        if (!transport_start->size() || !transport_end->size())
            return false;

        start = transport_start->getValue();
        end   = transport_end->getValue();

        return !start.isEmpty() && !end.isEmpty();
    }

  protected slots:
    void FreqTableChanged(const QString &freqtbl)
    {
        if (freqtbl == "us")
            atsc_modulation->setValue(0);
        else if (atsc_modulation->getValue() == "vsb8")
            atsc_modulation->setValue(1);

        ResetTransportRange();
    }

    void ModulationChanged(const QString &modulation)
    {
        ResetTransportRange();
    }

    void TransportRangeChanged(const QString&)
    {
        int a = transport_start->getValueIndex(transport_start->getValue());
        int b = transport_end->getValueIndex(transport_end->getValue());
        if (b < a)
        {
            transport_end->setValue(transport_start->getValue());
            b = a;
        }

        int diff = max(b + 1 - a, 0);
        transport_count->setValue(QString::number(diff));
    }

  protected:
    void ResetTransportRange(void)
    {
        transport_start->clearSelections();
        transport_end->clearSelections();
        transport_count->setValue(QString::number(0));

        FetchFrequencyTables();

        bool first = true;
        freq_table_list_t::iterator it = tables.begin();
        for (; it != tables.end(); ++it)
        {
            freq_table_list_t::iterator next = it;
            ++next;

            const FrequencyTable &ft = **it;
            int     name_num         = ft.name_offset;
            QString strNameFormat    = ft.name_format;
            uint    freq             = ft.frequencyStart;
            while (freq <= ft.frequencyEnd)
            {
                QString name = strNameFormat;
                if (strNameFormat.indexOf("%") >= 0)
                    name = strNameFormat.arg(name_num);

                transport_start->addSelection(name, name, first);
                first = false;

                bool last = (next == tables.end()) &&
                    ((freq + ft.frequencyStep) >= ft.frequencyEnd);
                transport_end->addSelection(name, name, last);

                name_num++;
                freq += ft.frequencyStep;
            }
        }
    }

    void FetchFrequencyTables(void)
    {
        QString format     = "atsc";
        QString modulation = GetModulation();
        QString country    = GetFrequencyTable();

        const QString new_tables_sig =
            QString("%1_%2_%3").arg(format).arg(modulation).arg(country);

        if (new_tables_sig != tables_sig)
        {
            while (!tables.empty())
            {
                delete tables.back();
                tables.pop_back();
            }

            tables_sig = new_tables_sig;

            tables = get_matching_freq_tables(
                format, modulation, country);
        }
    }

  protected:
    ScanFrequencyTable      *atsc_table;
    ScanATSCModulation      *atsc_modulation;
    TransComboBoxSetting    *transport_start;
    TransComboBoxSetting    *transport_end;
    TransLabelSetting       *transport_count;
    QString                  old_freq_table;
    QString                  tables_sig;
    freq_table_list_t        tables;
};

#endif // _PANE_ATSC_H_
