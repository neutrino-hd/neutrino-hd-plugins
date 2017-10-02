/*
 * $Id$
 *
 * (C) 2005 by digi_casi <digi_casi@tuxbox.org>
 *          based on vlc plugin by mechatron
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <plugin.h>
#include <xmltree.h>
#include "movieplayer.h"
#include <lib/dvb/decoder.h>
#include <lib/base/buffer.h>

#define REL "Movieplayer Plugin, v0.9.29"

extern "C" int plugin_exec(PluginParam *par);
extern eString getWebifVersion();
extern eIOBuffer tsBuffer;

bool sortByName(const PLAYLIST& a, const PLAYLIST& b)
{
	return a.Filename < b.Filename;
}

bool sortByType(const PLAYLIST& a, const PLAYLIST& b)
{
	if (a.Filetype == b.Filetype)
		return sortByName(a, b);
	else
		return a.Filetype < b.Filetype;
}

eSCGui::eSCGui(): menu(true)
{
	int fd = eSkin::getActive()->queryValue("fontsize", 20);

	pauseBox = new eMessageBox(_("Press yellow or green button for continue..."),_("Pause"), eMessageBox::iconInfo);

	cmove(ePoint(90, 110));
	cresize(eSize(540, 380));

	addActionMap(&i_shortcutActions->map);
	addActionMap(&i_cursorActions->map);

	list = new eListBox<eListBoxEntryText>(this);
	list->move(ePoint(10, 10));
	list->resize(eSize(clientrect.width() - 20, 260));
	CONNECT(list->selected, eSCGui::listSelected);
	CONNECT(list->selchanged, eSCGui::listSelChanged);

	eLabel *l_root = new eLabel(this);
	l_root->move(ePoint(10, clientrect.height() - 110));
	l_root->resize(eSize(130, fd + 5));
	l_root->setProperty("align", "center");
	l_root->setProperty("vcenter", "");
	l_root->setProperty("backgroundColor", "std_dred");
	l_root->setText(_("Files"));

	eLabel *l_vcd = new eLabel(this);
	l_vcd->move(ePoint(140, clientrect.height() - 110));
	l_vcd->resize(eSize(130, fd + 5));
	l_vcd->setProperty("align", "center");
	l_vcd->setProperty("vcenter", "");
	l_vcd->setProperty("backgroundColor", "std_dgreen");
	l_vcd->setText("SVCD&VCD");

	eLabel *l_dvd = new eLabel(this);
	l_dvd->move(ePoint(270, clientrect.height() - 110));
	l_dvd->resize(eSize(130, fd + 5));
	l_dvd->setProperty("align", "center");
	l_dvd->setProperty("vcenter", "");
	l_dvd->setProperty("backgroundColor", "std_dyellow");
	l_dvd->setText("DVD");

	eLabel *l_cfg = new eLabel(this);
	l_cfg->move(ePoint(400, clientrect.height() - 110));
	l_cfg->resize(eSize(130, fd + 5));
	l_cfg->setProperty("align", "center");
	l_cfg->setProperty("vcenter", "");
	l_cfg->setProperty("backgroundColor", "std_dblue");
	l_cfg->setText(_("Options"));

	status = new eStatusBar(this);
	status->move(ePoint(10, clientrect.height() - 70));
	status->resize(eSize(clientrect.width() - 20, 60));
	status->loadDeco();

	timer = new eTimer(eApp);
	CONNECT(timer->timeout, eSCGui::timerHandler);

	eMoviePlayer::getInstance()->mpconfig.load();
	server = eMoviePlayer::getInstance()->mpconfig.getServerConfig();

	VLC_IP = server.serverIP;
	VLC_IF_PORT = server.webifPort;

	startdir = server.startDir;
	cddrive = server.CDDrive;

	if (server.vlcUser && server.vlcPass)
		VLC_AUTH = server.vlcUser + ":" + server.vlcPass;

	int mode = DATA;
	eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/lastmode", mode);

	if(mode == DATA)
		getSavedPath();

	loadList(mode, startdir);
}

eSCGui::~eSCGui()
{
	playList.clear();
	delete timer;
	eDebug("[MOVIEPLAYER] ending...");
}

bool eSCGui::supportedFileType(eString filename)
{
	unsigned int pos = filename.find_last_of('.');
	eString extension = filename.right(filename.size() - pos - 1);
	struct videoTypeParms videoParms = eMoviePlayer::getInstance()->mpconfig.getVideoParms((eString)"File", extension);
	return videoParms.name == "File";
}

void eSCGui::loadList(int mode, eString pathfull)
{
	playList.clear();
	PLAYLIST a;

	infoBox = 0; bufferingBox = 0; jumpBox = 0; skip_time = 0;
	list->setHelpText(_("Please wait ... communicating with VLC"));

	eString errPath = " " + (eString) _("Error") + " - " + pathfull + _(" is not accessible...");
	eString upPath = /*"\t" +*/ (eString)_("[GO UP]");

	nFiles=0;
	int nGoUp=0;
	int isItem=0;

	eString tmp2 = " ";
	eString tmp3;

	switch (mode)
	{
		case DATA:
			tmp2 += _("Files");
			tmp3 = "";
			break;
		case VCD:
			tmp2 += "SVCD&VCD";
			setText(tmp2 + " - Drive: " + cddrive);
			tmp3 = "vcd:";
			break;
		case DVD:
			tmp2 += "DVD";
			setText(tmp2 + " - Drive: " + cddrive);
			tmp3 = "dvdsimple:";
			break;
	}

	eString response;
	CURLcode httpres= sendGetRequest("/requests/browse.xml?dir=" + pathfull.strReplace(" ", "%20"), response);

	//std::replace(response.begin(), response.end(), '\\', '/');  for auto-subtitles under Windows must be commented !!!
	response.strReplace("://", ":/"); // what the heck...
	if (httpres == 0 && response.length() > 0)
	{
		if (mode == DATA)
		{
//			eDebug("[IMS] file: %s",response.c_str());
			XMLTreeParser parser("UTF-8");

			unsigned int start = 0;
			for (unsigned int pos = response.find('\n', 0); pos != std::string::npos; pos = response.find('\n', start))
			{
				eString entry = ""; entry = response.substr(start, pos - start); // one line
				if(entry.length() > 5) // all less than <root> is skipped
				{
					if (!parser.Parse(entry.c_str(), entry.length(), 0))
					{
						//eDebug("[MOVIEPLAYER] parsing settings file: %s at line %d>",parser.ErrorString(parser.GetErrorCode()), parser.GetCurrentLineNumber());
						eMessageBox msg(eString("parser.RootNode() response"), _("Error"), eMessageBox::btOK);
						msg.show();msg.exec();msg.hide();
						return;
					}
					isItem++;
				}
				start = pos + 1;
			}
			if( isItem < 4 ) // if no items
			{
				a.Filename = upPath;
				a.Fullname = "/..";
				setText(errPath);
				a.Filetype = GOUP;
				playList.push_back(a);
			}

			XMLTreeNode *root_mp = parser.RootNode();
			if (!root_mp)
			{
//				eDebug("[MOVIEPLAYER] parsing error");
				eMessageBox msg(eString("parser.RootNode()"), _("Error"), eMessageBox::btOK);
				msg.show();msg.exec();msg.hide();
				return;
			}

			for (XMLTreeNode *node = root_mp->GetChild(); node; node = node->GetNext())
			{
//				eDebug("[MOVIEPLAYER] file: %s",entry.c_str());

				eString tmptype = node->GetAttributeValue("type");
				eString tmpsize = node->GetAttributeValue("size");
//				eString tmpdate = node->GetAttributeValue("date");  //  VLC returns date = size only :(
				eString tmppath = node->GetAttributeValue("path");
				eString tmpname = node->GetAttributeValue("name");

				if (tmptype =="directory")
				{
					if (tmpname == ".." || tmpname[1] == ':' && tmpname.length() == 2) // is GOUP or disk name
					{
						if( tmpname[1] == ':' )  // Win's disk name
						{
							a.Filename = tmpname;
							a.Fullname = tmppath;
							setText(tmp2 + " - " + _("Select drive:"));
							a.Filetype = GOUP;
							//eDebug("[MOVIEPLAYER] goup2: %s",a.Fullname.c_str());
							playList.push_back(a);
						}
						else if(!nGoUp) // VLC return for some net 2x root => used only one
						{
							a.Filename = upPath;
							a.Fullname = tmppath;
							eString display = a.Fullname.left(a.Fullname.length() - 3);
							path = tmp2 + " - " + _("Path:") + " " + display;
							setText(path);
							a.Filetype = GOUP;
							//eDebug("[MOVIEPLAYER] goup1: %s",a.Fullname.c_str());
							playList.push_back(a);
							nGoUp++;

							int savepath = 0;
							eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/savepath",savepath);
							if(savepath)
								eConfig::getInstance()->setKey("/enigma/plugins/movieplayer/path",display);
						}
					}
					else
					{
						a.Filename = "[DIR]  " + tmpname;
						a.Fullname = tmppath;
						a.Filetype = DIRS;
						//eDebug("[MOVIEPLAYER] dir: %s %s",a.Fullname.c_str(),a.Filename.c_str());
						playList.push_back(a);
					}
				}
				else if (tmptype == "file" )
				{
					if (supportedFileType(tmpname))
					{
						a.Filename = tmpname;
						a.Fullname = tmppath;
						a.Filesize = eString().sprintf("%s: %d%s",_("Size"), atoi(tmpsize.c_str())/1024/1024,"MB");
						a.Filetype = FILES;
						playList.push_back(a);
						nFiles++;
						eDebug("[MOVIEPLAYER] file name: %d %s %s", nFiles, a.Fullname.c_str(),a.Filesize.c_str());
					}
				}
			}
		}
		else //VCD,SVCD,DVD
		{
			if (cddrive[cddrive.length() - 1] == ':' && cddrive.length() == 2)
			{
				pathfull = cddrive + "\\";
			}

			for (int i = 1; i <= 30; i++)
			{
				a.Filename = (eString)_("Chapter") + eString().sprintf(" %02d", i);
				a.Fullname = tmp3 + cddrive + "@1:" + eString().sprintf("%d-", i);
				a.Filetype = FILES;
				playList.push_back(a);
				nFiles++;
			}
		}
	}
	else
	{
		if( mode == DATA && httpres == 52 ) // path is not accesible - Win (long time, then is returned httpres=52)
		{
			a.Filename = upPath;
			a.Fullname = pathfull + "/..";
			setText(errPath);
			a.Filetype = GOUP;
			playList.push_back(a);
		}
		else
			setText((eString)_(" Movieplayer could not communicate with VLC"));
		list->setHelpText(_("Please make sure that VLC is started and PC's IP address is configured correctly in the streaming settings."));
	}

	viewList();
	eConfig::getInstance()->setKey("/enigma/plugins/movieplayer/lastmode", mode);
}

void eSCGui::viewList()
{
	sort(playList.begin(), playList.end(), sortByType);

	list->beginAtomic();
	list->clearList();
	int current = 0;

	for (PlayList::iterator p = playList.begin(); p != playList.end(); p++)
		(*p).listEntry = new eListBoxEntryText(list, (*p).Filename, (void *)current++);

	list->endAtomic();
	setStatus(0);
}

void eSCGui::setStatus(int val)
{
	if (playList.size() >  0)
	{
		switch (playList[val].Filetype)
		{
			case GOUP:
				list->setHelpText(_("Go up one directory level"));
				break;
			case DIRS:
				list->setHelpText(_("Enter directory"));
				break;
			case FILES:
				list->setHelpText(playList[val].Filename + "\n" + playList[val].Filesize);
				break;
		}
	}
}

void eSCGui::listSelChanged(eListBoxEntryText *item)
{
	if (item)
		setStatus((int)item->getKey());
}

void eSCGui::listSelected(eListBoxEntryText *item)
{
	if (item)
	{
		if(menu)
			val = (int)item->getKey();
		if (playList[val].Filetype == FILES)
			playerStart(val);
		else
			loadList(DATA, playList[val].Fullname);
	}
}

void eSCGui::showMenu()
{
	if (!menu)
	{
		timer->stop();
		if(bufferingBox)
		{
			bufferingBox->hide();
			delete bufferingBox;
			bufferingBox=0;
		}
		pauseBox->hide();
		hide();
		cmove(ePoint(90, 110));
		status->loadDeco();
		show();

		menu = true;
	}
}

void eSCGui::timerHandler()
{
// 	eDebug("[MOVIEPLAYER PLUGIN] timerHandler: status = %d", eMoviePlayer::getInstance()->status.STAT);

	switch (eMoviePlayer::getInstance()->status.STAT)
	{
		case eMoviePlayer::STREAMERROR:
		{
//		        eDebug("#   STREAMERROR:");
			setText((eString)_(" A streaming error occurred..."));
			list->setHelpText(_("Please make sure that VLC is started on the PC and that it can play the file you selected."));
			int stop_err=0;
			eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/err", stop_err );
			if(stop_err)
			{
				showMenu();
				break;
			}
			else
			{
				eDebug("A streaming error occurred, trying next file...");
				if(bufferingBox)
				{
					bufferingBox->hide();
					delete bufferingBox;
					bufferingBox=0;
				}
			}
		}
		case eMoviePlayer::STOPPED:
		{
//		        eDebug("#   STOPPED:");
			int play_next=1;
			eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/playnext", play_next );

			if (playList.size() > 1 && play_next)
			{
				do
				{
					if (++val >= playList.size())
			 			val = 0;
		 		} while (playList[val].Filetype != FILES);

				playerStart(val);
				list->setCurrent(playList[val].listEntry);
			}
			else
				showMenu();
			break;
		}
		default:
		{       
			if(infoBox)
			{
				eString text = "";
				infoBox->setText(filePos(0,"","",text));
			}

			if (!eMoviePlayer::getInstance()->status.BUFFERFILLED)
			{
				if(infoBox)
				{
					infoBox->hide();
					delete infoBox;
					infoBox=0;
				}
				if(!bufferingBox)
				{
					bufferingBox = new eMessageBox(_("Buffering video stream... Please wait."),"Movieplayer", eMessageBox::iconInfo);
					bufferingBox->show();
				}
				else
				{
					int a = tsBuffer.size()/(eMoviePlayer::getInstance()->initialBuffer/100);
					bufferingBox->setText(eString().sprintf("Movieplayer - %s:  %d%c",_("finished"),a,'%'));
				}
			}
			else
			{
				if(bufferingBox)
				{
					bufferingBox->hide();
					delete bufferingBox;
					bufferingBox=0;
				}
			}
			if(jumpBox)
			{
				int msgTime=3;
				eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/msgtime", msgTime );
				if(++skip_time >= msgTime)
				{
					jumpBox->hide();
					delete jumpBox;
					jumpBox=0;
					skip_time = 0;
				}
			}
			timer->start(1000, true);
			break;
		}
	}
}

void eSCGui::playerStart(int val)
{
	if (menu)
	{
		hide();
		cmove(ePoint(90, 800));
		show();
		menu = false;

	}

	eDebug("\n[VLC] play %d \"%s\"", val, playList[val].Fullname.c_str());

	if (eMoviePlayer::getInstance()->status.STAT != eMoviePlayer::STOPPED &&
		eMoviePlayer::getInstance()->status.STAT != eMoviePlayer::STREAMERROR)
	{
		if(eMoviePlayer::getInstance()->status.BUFFERFILLED) // info about played file - OK button:
		{
			if(infoBox)
			{
				infoBox->hide();
				delete infoBox;
				infoBox=0;
			}
			else
			{
				eString text="";
				eString caption = filePos(1,playList[val].Filename,playList[val].Filesize, text);
				infoBox = new eMessageBox( text, caption, eMessageBox::iconInfo );
				infoBox->show();
			}
		}
	}
	else
	{
		changeSout();  // change some sout parameters
		setText(path); // refresh title
		eMoviePlayer::getInstance()->control("start2", playList[val].Fullname.c_str());
		timer->start(2000, true);
	}
}

eString eSCGui::filePos(int both, eString name, eString size, eString& text)
{
	eString restmp="";
	sendGetRequest("/requests/status.xml?command=", restmp);
	int l = atoi(getPar(restmp,"<length>").c_str());
	int t = atoi(getPar(restmp,"<time>").c_str());
	if(both)
		text = eString().sprintf("%s\n\n%s: %d:%02d\t%s",name.c_str(),_("Duration"), l/60, l%60, size.c_str());
	eString caption = eString().sprintf("%s%s:  %d:%02d", "Movieplayer - ", _("position in played file"), t/60, t%60);
	return caption;
}

void eSCGui::changeSout()
{
	int async = 1;
	eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/async", async );
	if(!async)
		eMoviePlayer::getInstance()->control("async", ""); // play without audio-sync

	int subtitles = 1;
	eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/sub", subtitles );
	if(!subtitles)
		eMoviePlayer::getInstance()->control("subtitles", ""); // play without subtitles

	int nsf = 0;
	eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/nsf", nsf);
	if(nsf)
		eMoviePlayer::getInstance()->control("nsf", ""); // can play file without A or V
}

// Help message, positions of jump 0-9 are used for jump message in eventHandler !
static char *NAME[] =
{
	_("red:\tRewind 5 percent"),
	_("green:\tPlay/Resync"),
	_("yellow:\tPause -max cca 8s"),
	_("blue:\tFast forward 5 percent/Options"),
	_("OK:\tPlay/Info about played file"),
	_("1:\tRewind 15 seconds"),
	_("2:\tGo to 20 percent"),
	_("3:\tFast forward 15 seconds"),
	_("4:\tRewind 1 minute"),
	_("5:\tGo to 50 percent"),
	_("6:\tFast forward 1 minute"),
	_("7:\tRewind 5 minutes"),
	_("8:\tGo to 80 percent"),
	_("9:\tFast forward 5 minutes"),
	_("0:\tRewind to begin"),
	_("up:\tNext item"),
	_("down:\tPrevious item"),
	_("exit:\tCancel playback"),
	_("menu:\tOptions if play"),
	" "
};

void eSCGui::pause()
{
	if (eMoviePlayer::getInstance()->status.STAT == eMoviePlayer::PLAY)
	{
		Decoder::Pause(2); // freeze
	}
	else
	{
		Decoder::Resume(true);
	}
//	Decoder::setAutoFlushScreen(0);
//	Decoder::setAutoFlushScreen(1);
}

int eSCGui::eventHandler(const eWidgetEvent &e)
{
	int jump = 0;
	eString command  = "/requests/status.xml?command=";
	eString restmp = "";

	switch (e.type)
	{
		case eWidgetEvent::evtAction:
		if (e.action == &i_cursorActions->cancel)
		{
			if(infoBox)
			{
				infoBox->hide();
				delete infoBox;
				infoBox=0;
			}
			else
			{
				if (eMoviePlayer::getInstance()->status.STAT != eMoviePlayer::STOPPED)
					eMoviePlayer::getInstance()->control("stop", "");

				if (!menu)
					showMenu();
				else
				{
					// set back DVB
					if(eMoviePlayer::getInstance()->status.DVB)
//					{
//						eMoviePlayer::getInstance()->control("dvbon", "");
					    eMoviePlayer::getInstance()->startDVB();
//					}
					eMoviePlayer::getInstance()->control("endplg", "");
					eMoviePlayer::getInstance()->supportPlugin();
					close(0);
				}
			}
		}
		else
		if (e.action == &i_cursorActions->help)
		{
			if(!bufferingBox)
			{
				if (menu)
					hide();
    				eSCGuiHelp help;
    				help.show();
    				help.exec();
    				help.hide();
    				if (menu)
					show();
    			}
    		}
		else
		if (e.action == &i_shortcutActions->red)
		{
			if (menu)
			{
				getSavedPath();
				loadList(DATA, startdir);
			}
			else
			{
				if(!bufferingBox)
				{
					command+="seek&val=%2d5%";sendGetRequest(command, restmp);
					//eMoviePlayer::getInstance()->control("jump", "");
					if(!jumpBox)
					{
						jumpBox = new eMessageBox(_("Rewind 5 percent"),_("Skipping"), eMessageBox::iconInfo);
						jumpBox->show();
					}
				}
			}
		}
		else
		if (e.action == &i_shortcutActions->green)
		{
			if (menu)
				loadList(VCD, "");
			else
			{
				if(!bufferingBox)
				{
    					if (eMoviePlayer::getInstance()->status.STAT == eMoviePlayer::PLAY)
    					{
    						command+="pl_pause";
    						for( int i = 0; i < 2; i++)  // pseudo resync
    						{
    							sendGetRequest(command, restmp); //pause
    							usleep(10000);
    							sendGetRequest(command, restmp); //play
    							usleep(10000);
    						}
    						eMoviePlayer::getInstance()->control("resync", "");
    					}
    					else
    					{
    						command+="pl_pause";sendGetRequest(command, restmp);
    						pause();
    						eMoviePlayer::getInstance()->control("play", "");
    						pauseBox->hide();
    					}
    				}
    			}
		}
		else
		if (e.action == &i_shortcutActions->yellow)
		{
			if (menu)
				loadList(DVD, "");
			else
			{
    				if(!bufferingBox)
				{
					command+="pl_pause";sendGetRequest(command, restmp);
					pause();
    					if (eMoviePlayer::getInstance()->status.STAT == eMoviePlayer::PAUSE)
    					{
    						eMoviePlayer::getInstance()->control("play", "");
    						pauseBox->hide();
    					}
    					else
    					{
    						eMoviePlayer::getInstance()->control("pause", "");
    						pauseBox->show();
    					}
    				}
    			}
		}
		else
		if (e.action == &i_shortcutActions->blue)
		{
			if (menu)
			{
				hide();
				eSCGuiConfig cfg;
				cfg.show();
				cfg.exec();
				cfg.hide();
				show();
			}
			else
			{
				if(!bufferingBox)
				{
					command+="seek&val=%2b5%";sendGetRequest(command, restmp);
					if(!jumpBox)
					{
						jumpBox = new eMessageBox(_("Fast forward 5 percent"),_("Skipping"), eMessageBox::iconInfo);
						jumpBox->show();
					}
					//eMoviePlayer::getInstance()->control("jump", "");
				}
			}
		}
		else
		if (e.action == &i_shortcutActions->menu)
		{
			if(!bufferingBox)
			{
				if (menu)
					hide();
				eSCGuiConfig cfg;
				cfg.show();
				cfg.exec();
				cfg.hide();
				if (menu)
					show();
			}
		}
/*		if (e.action == &i_cursorActions->left)
		{
			if (!menu)
				eMoviePlayer::getInstance()->control("left", "");
			else
				return eWindow::eventHandler(e);
		}
		else
		if (e.action == &i_cursorActions->right)
		{
			if (!menu)
				eMoviePlayer::getInstance()->control("right", "");
			else
				return eWindow::eventHandler(e);
		}
*/		else
		if (e.action == &i_cursorActions->up)
		{
			if (!menu && !bufferingBox)
			{
				if(infoBox)
				{
					infoBox->hide();
					delete infoBox;
					infoBox=0;
				}
				eMoviePlayer::getInstance()->control("up", "");
				if(val<nFiles)
				{
					if (eMoviePlayer::getInstance()->status.STAT != eMoviePlayer::STOPPED)
						eMoviePlayer::getInstance()->control("stop", "");
							changeSout();
					eMoviePlayer::getInstance()->control("start2", playList[++val].Fullname.c_str());
					timer->start(2000, true);
				}
			}
			else
				return eWindow::eventHandler(e);
		}
		else
		if (e.action == &i_cursorActions->down)
		{
			if (!menu && !bufferingBox)
			{
				if(infoBox)
				{       
					infoBox->hide();
					delete infoBox;
					infoBox=0;
				}
				eMoviePlayer::getInstance()->control("down", "");
				if(val > 0 )
				{
					eMoviePlayer::getInstance()->control("forward", "");
					if (eMoviePlayer::getInstance()->status.STAT != eMoviePlayer::STOPPED)
						eMoviePlayer::getInstance()->control("stop", "");
					changeSout();
					eMoviePlayer::getInstance()->control("start2", playList[--val].Fullname.c_str());
					timer->start(2000, true);
				}
			}
			else
				return eWindow::eventHandler(e);
		}
		else
		if (e.action == &i_shortcutActions->number1)
		{
			if (!menu)
				jump = 1;
		}
		else
		if (e.action == &i_shortcutActions->number2)
		{
			if (!menu)
				jump = 2;
		}
		else
		if (e.action == &i_shortcutActions->number3)
		{
			if (!menu)
				jump = 3;
		}
		else
		if (e.action == &i_shortcutActions->number4)
		{
			if (!menu)
				jump = 4;
		}
		else
		if (e.action == &i_shortcutActions->number5)
		{
			if (!menu)
				jump = 5;
		}
		else
		if (e.action == &i_shortcutActions->number6)
		{
			if (!menu)
				jump = 6;
		}
		else
		if (e.action == &i_shortcutActions->number7)
		{
			if (!menu)
				jump = 7;
		}
		else
		if (e.action == &i_shortcutActions->number8)
		{
			if (!menu)
				jump = 8;
		}
		else
		if (e.action == &i_shortcutActions->number9)
		{
			if (!menu)
				jump = 9;
		}
		else
		if (e.action == &i_shortcutActions->number0)
		{
			if (!menu)
				jump = 10;
		}
		else
			break;

		if (jump > 0 )
		{
			switch(jump)
			{
				// forward relative
				case 1:	command+="seek&val=%2d15"; break;
				case 4: command+="seek&val=%2d60"; break;
				case 7: command+="seek&val=%2d5m"; break;
				// go to position in %
				case 2: command+="seek&val=20%"; break;
				case 5: command+="seek&val=50%"; break;
				case 8: command+="seek&val=80%"; break;
				// rewind relative
				case 3: command+="seek&val=%2b15"; break;
				case 6: command+="seek&val=%2b60"; break;
				case 9: command+="seek&val=%2b5m"; break;
				// rewind to begin
				case 10:command+="seek"; break;
			}
			if(!bufferingBox)
			{
    				sendGetRequest(command, restmp);
    				eString tmp = eString().sprintf(NAME[jump+4],jump) ;
    				eMoviePlayer::getInstance()->control("jump", eString().sprintf("%d", jump).c_str());
    				if(!jumpBox)
    				{
					jumpBox = new eMessageBox(tmp.strReplace(":\t", " - "),_("Skipping"), eMessageBox::iconInfo);
					jumpBox->show();
    				}
    			}
		}
		return 1;
	default:
		break;
	}
	return eWindow::eventHandler(e);
}

eSCGuiHelp::eSCGuiHelp()  // Help window
{
	cmove(ePoint(90, 80));
	cresize(eSize(540, 440));

	eString rel = REL;
	setText((eString)_("Help")+" - "+rel);

	list = new eListBox<eListBoxEntryText>(this);
	list->move(ePoint(10, 10));
	list->resize(eSize(clientrect.width() - 20, clientrect.height() - 20));

//	new eListBoxEntryText(list, REL);
	int i = 0;
	while (eString(NAME[i]) != " ")
		new eListBoxEntryText(list, NAME[i++]);
}

size_t CurlDummyWrite (void *ptr, size_t size, size_t nmemb, void *data)
{
	std::string *pStr = (std::string *)data;
	*pStr += (char *)ptr;
	return size * nmemb;
}

CURLcode eSCGui::sendGetRequest (const eString& url, eString& response)  // send http commands to VLC, in response return info
{
	CURL *curl;
	CURLcode httpres;

	eString tmpurl= "http://" + VLC_IP + ":" + VLC_IF_PORT + url;
	response = "";

	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, tmpurl.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlDummyWrite);
	curl_easy_setopt(curl, CURLOPT_FILE, (void *)&response);

	if (VLC_AUTH)
		curl_easy_setopt (curl, CURLOPT_USERPWD, VLC_AUTH.c_str());
	curl_easy_setopt (curl, CURLOPT_FAILONERROR, true);

	int timeout = 5;
	eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/timeout", timeout);
//	eDebug("[VLC] Timeout: %d",timeout);
	curl_easy_setopt (curl, CURLOPT_TIMEOUT, timeout);
	httpres = curl_easy_perform (curl);
//	eDebug("[VLC] HTTP Result: %d", httpres);
	curl_easy_cleanup(curl);
	return httpres;
}

void eSCGui::getSavedPath()
{
	int savepath=0;
	eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/savepath", savepath);
	if(savepath)
	{
		char* temp = 0;
		if (!eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/path", temp))
		{
			startdir = temp;
			free(temp);
		}
	}
}

eString eSCGui::getPar(eString buf, const char* parameter)
{
/* parameter can be:
--------------------
<time>
<length>
<volume>
<state>
<position>
<state>
<random>
<loop>
<repeat> */

        eString par = "";
//        eDebug("[getPar] entry: %s",buf.c_str());
	unsigned int start = 0;
	for (unsigned int pos = buf.find('\n', 0); pos != std::string::npos; pos = buf.find('\n', start))
	{
		eString entry = ""; entry = buf.substr(start, pos - start); // one line
		if(entry.find(parameter) != eString::npos)
		{
			par = entry.strReplace(parameter,"");
//                      eDebug("[getPar] parameter: %s",par.c_str());
			par = par.left(par.find_last_of('<')).strReplace(" ","");
			break;
		}
		start = pos + 1;
	}
	eDebug("[getPar] parameter: %s par: %s",parameter,par.c_str());
	return par;
}

eSCGuiConfig::eSCGuiConfig(): eWindow()  // Config window
{
	cmove(ePoint(120, 80));
	setText(_("Options"));
	resize(eSize( 450, 480));
	int yPos = 10;
	int fd = eSkin::getActive ()->queryValue ("fontsize", 16) + 8;	
	
	play_next = 1;
	eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/playnext", play_next );

	int Timeout = 5;
	eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/timeout", Timeout);

	int msgTime = 3;
	eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/msgtime", msgTime );

	int savepath = 0;
	eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/savepath", savepath );

	int stop_err = 0;
	eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/err", stop_err );

	int async = 1;
	eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/async", async );

	int subtitles = 1;
	eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/sub", subtitles );

	int resdvb = 1;
	eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/resdvb", resdvb );

	int pbuf = 100;
	eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/pbuf", pbuf);

	int nsf = 0;
	eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/nsf", nsf);

	playNext=new eCheckbox(this, play_next, 1);
	playNext->setText(_("Continuous playback"));
	playNext->move(ePoint(10, yPos));
	playNext->resize(eSize(260, fd));
	playNext->setHelpText(_("Playback next file from playlist"));

	yPos +=(35);

	stopErr=new eCheckbox(this, stop_err, 1);
	stopErr->setText(_("Stop after error"));
	stopErr->move(ePoint(10, yPos));
	stopErr->resize(eSize(260, fd));
	stopErr->setHelpText(_("Don't playback next file from list if previous playback failed"));
	yPos +=(35);

	savePath=new eCheckbox(this, savepath, 1);
	savePath->setText(_("Last directory path"));
	savePath->move(ePoint(10, yPos));
	savePath->resize(eSize(260, fd));
	savePath->setHelpText(_("Use last path or path from .xml file"));

	yPos +=(35);

	aSync=new eCheckbox(this, async, 1);
	aSync->setText(_("Audio-sync"));
	aSync->move(ePoint(10, yPos));
	aSync->resize(eSize(260, fd));
	aSync->setHelpText(_("Use audio-sync parameter for playback"));

	yPos +=(35);

	subTitles=new eCheckbox(this, subtitles, 1);
	subTitles->setText(_("Auto Subtitles"));
	subTitles->move(ePoint(10, yPos));
	subTitles->resize(eSize(260, fd));
	subTitles->setHelpText(_("Use autoselection of subtitles by VLC"));

	if (eMoviePlayer::getInstance()->status.STAT != eMoviePlayer::PLAY)
	{
		yPos +=(35);

		resDVB=new eCheckbox(this, resdvb, 1);
		resDVB->setText(_("Restoring DVB"));
		resDVB->move(ePoint(10, yPos));
		resDVB->resize(eSize(260, fd));
		resDVB->setHelpText(_("Restoring DVB after each played file"));
		CONNECT (resDVB->checked, eSCGuiConfig::setDVB);
	}

	yPos +=(35);

	setNsf=new eCheckbox(this, nsf, 1);
	setNsf->setText(_("Non-standard file"));
	setNsf->move(ePoint(10, yPos));
	setNsf->resize(eSize(260, fd));
	setNsf->setHelpText(_("Can playback file with missing audio or video"));


	yPos +=(35);

	lNrSec = new eLabel(this);
	lNrSec->move(ePoint(10, yPos));
	lNrSec->resize(eSize(200, fd));
	lNrSec->setText(_("Timeout in seconds:"));
	lNrSec->loadDeco();

	comNrSec = new eComboBox(this, 3, lNrSec);
	comNrSec->move(ePoint(210, yPos));
	comNrSec->resize(eSize (60, fd));
	comNrSec->setHelpText(_("How long is tested comunication with VLC"));
	comNrSec->loadDeco();

	for(int time = 1; time <= 5; ++time)
	{
		new eListBoxEntryText(*comNrSec, eString().sprintf("%d", time), (void*)(time));
	}
	comNrSec->setCurrent((void *)Timeout);

	yPos +=(35);

	lbuff = new eLabel(this);
	lbuff->setText(_("Buffer size:"));
	lbuff->move(ePoint(10, yPos));
	lbuff->resize(eSize(120, fd));

	sBuff = new eSlider( this, lbuff, 10, 100 );
	sBuff->setIncrement( 10 ); // percent
	sBuff->move( ePoint( 120, yPos ) );
	sBuff->resize(eSize( 220, fd) );
	sBuff->setHelpText(_("Size of streaming buffer (left, right)"));
	sBuff->setValue(pbuf);
	CONNECT( sBuff->changed, eSCGuiConfig::BuffChanged );

	lbuff1 = new eLabel(this);
	lbuff1->setText(eString().sprintf("%d%c",pbuf,'%'));
	lbuff1->move(ePoint(120+225, yPos));
	lbuff1->resize(eSize(50, fd));


	yPos +=(35);

	lmsgTime = new eLabel(this);
	lmsgTime->move(ePoint(10, yPos));
	lmsgTime->resize(eSize(210, fd));
	lmsgTime->setText(_("Skip message delay:"));
	lmsgTime->loadDeco();

	comMsgTime = new eComboBox(this, 3, lmsgTime);
	comMsgTime->move(ePoint(210, yPos));
	comMsgTime->resize(eSize (60, fd));
	comMsgTime->setHelpText(_("How long is displayed message about skipping (in seconds)"));
	comMsgTime->loadDeco();

	for(int time = 1; time <= 10; ++time)
	{
		new eListBoxEntryText(*comMsgTime, eString().sprintf("%d", time), (void*)(time));
	}
	comMsgTime->setCurrent((void *)msgTime);

	eButton* bOK = new eButton(this);
	bOK->setText(_("Save"));
	bOK->setShortcut("green");
	bOK->setShortcutPixmap("green");
	bOK->move(ePoint (10, clientrect.height()-70));
	bOK->resize(eSize (120, 30));
	bOK->setHelpText(_("Save changes and return"));
	bOK->loadDeco();
	
	CONNECT (bOK->selected, eSCGuiConfig::okPressed);

	eStatusBar* sStatusbar = new eStatusBar(this);
	sStatusbar->move(ePoint (0, clientrect.height() - 30));
	sStatusbar->resize(eSize(clientrect.width(), 30));
	sStatusbar->loadDeco();
}

void eSCGuiConfig::okPressed()
{
	eConfig::getInstance()->setKey("/enigma/plugins/movieplayer/playnext", playNext->isChecked() ? 1 : 0 );
	eConfig::getInstance()->setKey("/enigma/plugins/movieplayer/savepath", savePath->isChecked() ? 1 : 0);
 	eConfig::getInstance()->setKey("/enigma/plugins/movieplayer/err", stopErr->isChecked() ? 1 : 0);
 	eConfig::getInstance()->setKey("/enigma/plugins/movieplayer/timeout", (const int)comNrSec->getCurrent()->getKey());
 	eConfig::getInstance()->setKey("/enigma/plugins/movieplayer/msgtime", (const int)comMsgTime->getCurrent()->getKey());
 	eConfig::getInstance()->setKey("/enigma/plugins/movieplayer/sub", subTitles->isChecked() ? 1 : 0 );
	eConfig::getInstance()->setKey("/enigma/plugins/movieplayer/async", aSync->isChecked() ? 1 : 0);
	eConfig::getInstance()->setKey("/enigma/plugins/movieplayer/resdvb", resDVB->isChecked() ? 1 : 0);
	eConfig::getInstance()->setKey("/enigma/plugins/movieplayer/pbuf", (const int)sBuff->getValue());
	eConfig::getInstance()->setKey("/enigma/plugins/movieplayer/nsf", setNsf->isChecked() ? 1 : 0);

	eMoviePlayer::getInstance()->control("bufsize", eString().sprintf("%d", (const int)sBuff->getValue()).c_str());

	close(0);
}

void eSCGuiConfig::BuffChanged( int i )
{
//        eDebug("eSlider %d",i);
	lbuff1->setText(eString().sprintf("%d%c",i,'%'));
}

void eSCGuiConfig::setDVB(int status)
{
	if (status)
	{
		if( eMoviePlayer::getInstance()->status.DVB )
		{
			eMoviePlayer::getInstance()->control("dvbon", "");
			eMoviePlayer::getInstance()->startDVB();
		}
	}
	else
	{
		eMoviePlayer::getInstance()->control("dvboff", "");
		eMoviePlayer::getInstance()->stopDVB();
	}
}

int plugin_exec(PluginParam *par)
{
	eSCGui dlg;
	eString webifVersion = getWebifVersion();
	if (webifVersion.find("Expert") != eString::npos)
	{
		eMoviePlayer::getInstance()->control("runplg", "");
		int resdvb = 1;
		eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/resdvb", resdvb );
		if(!resdvb)
		{
			eMoviePlayer::getInstance()->stopDVB();
			eMoviePlayer::getInstance()->control("dvboff", "");

		}
		int pbuf = 100;
		eConfig::getInstance()->getKey("/enigma/plugins/movieplayer/pbuf", pbuf );
		eMoviePlayer::getInstance()->control("bufsize", eString().sprintf("%d", pbuf).c_str());
		dlg.show();
		dlg.exec();
		dlg.hide();
	}
	else
	{
		eMessageBox msg(eString("This plugin requires the EXPERT version of the web interface to be installed.\nInstalled web interface version: " + webifVersion), _("Error"), eMessageBox::btOK);
		msg.show();
		msg.exec();
		msg.hide();
	}
	return 0;
}
