/*
	jobdialog.cpp

	Copyright (c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <iostream>
using namespace std;


#include "jobdialog.h"
#include "mfdinfo.h"
#include <mfdclient/mfdinterface.h>

JobDialog::JobDialog(
                        MythMainWindow *parent, 
                        QString window_name,
                        QString theme_filename,
                        MfdInfo *l_mfd_info,
                        MfdInterface *l_mfd_interface
                    )
          :MythThemedDialog(parent, window_name, theme_filename)
{
    mfd_info = l_mfd_info;
    mfd_interface = l_mfd_interface;
    wireUpTheme();
    current_job = -1;    
    checkJobs();
}

void JobDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    switch(e->key())
    {
        case Key_Left:
        
            prevjob_button->push();
            handled = true;
            break;
            
        case Key_Right:
        
            nextjob_button->push();
            handled = true;
            break;
            
        case Key_9:
            
            cancel_button->push();
            handled = true;
            break;
            
        case Key_T:
            
            this->close();
            handled = true;
            break;
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void JobDialog::checkJobs()
{
    if(current_job < 0 || !mfd_info)
    {
        tryToShowFirstJob();
    }
    else
    {
        //
        //  See if there's new information about the current job
        //
        QPtrList<JobTracker> *job_list = mfd_info->getJobList();
        int counter = 0;
        JobTracker *which_one = NULL;
        JobTracker *hunter = NULL;
        for( hunter = job_list->first(); hunter; hunter = job_list->next())
        {
            counter++;
            if(hunter->getId() == current_job)
            {
                which_one = hunter;
                break;
            }
        }
        if(which_one)
        {
            major_description->SetText(which_one->getMajorDescription());
            minor_description->SetText(which_one->getMinorDescription());
            major_progress->SetUsed(which_one->getMajorProgress());
            minor_progress->SetUsed(which_one->getMinorProgress());

            setNumbJobs(counter, job_list->count());
        }
        else
        {
            tryToShowFirstJob();
        }
    }

    major_description->refresh();
    minor_description->refresh();
    cancel_button->refresh();
}

void JobDialog::tryToShowFirstJob()
{
    if(mfd_info)
    {
        QPtrList<JobTracker> *job_list = mfd_info->getJobList();
        if(job_list->count() > 0)
        {
            setContext(2);
            JobTracker *which_one = job_list->first();
            current_job = which_one->getId();
            major_description->SetText(which_one->getMajorDescription());
            minor_description->SetText(which_one->getMinorDescription());
            major_progress->SetUsed(which_one->getMajorProgress());
            minor_progress->SetUsed(which_one->getMinorProgress());
            
            major_description->refresh();
            minor_description->refresh();
            
            setNumbJobs(1, job_list->count());
        }
        else
        {
            setContext(1);
            description_text->SetText("No jobs.");
            description_text->refresh();
        }
    }
    else
    {
        setContext(1);
        description_text->SetText("Not only no jobs, but no mfds.");
    }
}

void JobDialog::setNumbJobs(int job_numb, int numb_total)
{
    if(job_numb > 1)
    {
        prevjob_button->SetContext(2);
    }
    else
    {
        prevjob_button->SetContext(-2);
    }
    if(job_numb < numb_total)
    {
        nextjob_button->SetContext(2);
    }
    else
    {
        nextjob_button->SetContext(-2);
    }
    numb_jobs_text->SetText(QString("Job %1 of %2").arg(job_numb).arg(numb_total));

    prevjob_button->refresh();
    nextjob_button->refresh();
    numb_jobs_text->refresh();
}


void JobDialog::cancelJob()
{
    if(current_job > 0 && mfd_interface && mfd_info)
    {
        mfd_interface->cancelJob(mfd_info->getId(), current_job);
    }
}

void JobDialog::nextJob()
{
    cout << "I should be moving to the next job" << endl;
}

void JobDialog::prevJob()
{
    cout << "I whould be moving to the previous job" << endl;
}

void JobDialog::wireUpTheme()
{

    description_text = getUITextType("description");
    if(!description_text)
    {
        cerr << "jobdialog.o: Couldn't find a text type called description in your theme" << endl;
        exit(0);
    }

    numb_jobs_text = getUITextType("numb_jobs_text");
    if(!numb_jobs_text)
    {
        cerr << "jobdialog.o: Couldn't find a text type called numb_jobs_text in your theme" << endl;
        exit(0);
    }

    major_description = getUITextType("major_description");
    if(!major_description)
    {
        cerr << "jobdialog.o: Couldn't find a text type called major_description in your theme" << endl;
        exit(0);
    }
    
    minor_description = getUITextType("minor_description");
    if(!minor_description)
    {
        cerr << "jobdialog.o: Couldn't find a text type called minor_description in your theme" << endl;
        exit(0);
    }

    major_progress = getUIStatusBarType("major_progress");
    if(major_progress)
    {
        major_progress->SetTotal(100);
        major_progress->SetUsed(0);
    }
    else
    {
        cerr << "jobdialog.o: Couldn't find a status bar type called major_progress in your theme" << endl;
        exit(0);
    }

    minor_progress = getUIStatusBarType("minor_progress");
    if(minor_progress)
    {
        minor_progress->SetTotal(100);
        minor_progress->SetUsed(0);
    }
    else
    {
        cerr << "jobdialog.o: Couldn't find a status bar type called minor_progress in your theme" << endl;
        exit(0);
    }

    cancel_button = getUITextButtonType("cancel_button");
    if(cancel_button)
    {
        cancel_button->setText(tr("9 Cancel Job"));
        connect(cancel_button, SIGNAL(pushed()), this, SLOT(cancelJob()));
    }
    else
    {
        cerr << "jobdialog.o: Couldn't find a text button called cancel_button in your theme" << endl;
        exit(0);
    }

    nextjob_button = getUIPushButtonType("job_next_button");
    if(nextjob_button)
    {
        connect(nextjob_button, SIGNAL(pushed()), this, SLOT(nextJob()));
    }
    else
    {
        cerr << "jobdialog.o: Couldn't find a push button called job_next_button in your theme" << endl;
        exit(0);
    }
    prevjob_button = getUIPushButtonType("job_prev_button");
    if(prevjob_button)
    {
        connect(prevjob_button, SIGNAL(pushed()), this, SLOT(prevJob()));
    }        
    else
    {
        cerr << "jobdialog.o: Couldn't find a push button called job_prev_button in your theme" << endl;
        exit(0);
    }
}


JobDialog::~JobDialog()
{
}

