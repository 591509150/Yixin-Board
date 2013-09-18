#define _CRT_SECURE_NO_WARNINGS /* for vs */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <cairo.h>
#include <gtk/gtk.h>
#include "yixin.h"

typedef long long I64;
#define MAX_SIZE 20
#define OPENBOOK_SIZE 8000000
typedef struct
{
	I64 zobkey;
	char status;
} BOOK;
BOOK openbook[OPENBOOK_SIZE];
int openbooknum = 0;
int boardsize = 15;
int inforule = 0;
int specialrule = 0;
int timeoutturn = 5000;
int timeused = 0;
int timestart = 0;
int timeoutmatch = 1000000;
int computerside = 0; /* 0�� 1�� 2�� 3˫�� */
int board[MAX_SIZE][MAX_SIZE];
int boardnumber[MAX_SIZE][MAX_SIZE];
int movepath[MAX_SIZE*MAX_SIZE];
int forbid[MAX_SIZE][MAX_SIZE];
int piecenum = 0;
char isthinking = 0, isgameover = 0, isneedrestart = 0, isneedomit = 0;
int useopenbook = 1;
int levelchoice = 4;
int shownumber = 1;
int language = 0; /* 0: English 1: Chinese */
int movx[8] = {  0,  0,  1, -1,  1,  1, -1, -1}; /* ˳������ʤ���ĺ����й� */
int movy[8] = {  1, -1,  0,  0,  1, -1,  1, -1}; 
/* engine */
GIOChannel *iochannelin, *iochannelout, *iochannelerr;
/* windowmain */
GtkWidget *windowmain;
GtkWidget *tableboard;
GtkWidget *imageboard[MAX_SIZE][MAX_SIZE];
GtkWidget *labelboard[2][MAX_SIZE];
GtkWidget *vboxwindowmain;
GdkPixbuf *pixbufboard[9][7];
GdkPixbuf *pixbufboardnumber[9][7][MAX_SIZE*MAX_SIZE+1][2];
int imgtypeboard[MAX_SIZE][MAX_SIZE];
char piecepicname[80] = "piece.bmp";

char * _T(char *s)
{
	return g_locale_to_utf8(s, -1, 0, 0, 0);
}

void show_thanklist()
{
	printf(language==0?"I would like to thank my contributors, whose support makes yixin what it is.\n":
			(language==1?"��л��Щ�����ĵ�������д�ṩ֧�ְ��������ѣ������ǣ�\n":""));
	printf("  �˷�\n");
	printf("  XR\n");
	printf("  ���\n");
	printf("  ���з���\n");
	printf("  Tuyen Do\n");
	printf("  �ʹ�����\n");
	printf("  Saturn|Titan\n");
	printf("  Ԫ\n");
	printf("  TZ\n");
	printf("\n");
}

GdkPixbuf *draw_overlay(GdkPixbuf *pb, int w, int h, gchar *text, char *color)
{ 
	GdkPixmap *pm;
	GdkGC *gc;
	GtkWidget *scratch;
	PangoLayout *layout;
	gchar *markup;
	GdkPixbuf *ret;
	gchar format[100];
	
	pm = gdk_pixmap_new(NULL, w, h, 24);
	gdk_drawable_set_colormap(pm, gdk_colormap_get_system());
	gc = gdk_gc_new(pm); 
	gdk_draw_pixbuf(pm, gc, pb, 0, 0, 0, 0, w, h, GDK_RGB_DITHER_NONE, 0, 0);
	scratch = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize(scratch);
	layout = gtk_widget_create_pango_layout(scratch, NULL);
	gtk_widget_destroy(scratch);
	sprintf(format, "<big><b><span foreground='%s'>%%s</span></b></big>", color);
	markup = g_strdup_printf(format, text);
	pango_layout_set_markup(layout, markup, -1);
	g_free(markup);
	gdk_draw_layout(pm, gc, w/2-strlen(text)*4, h/2-10, layout);
	g_object_unref(layout); 
	ret = gdk_pixbuf_get_from_drawable(NULL, pm, NULL, 0, 0, 0, 0, w, h);
	return ret;
}

void send_command(char *command)
{
	gsize size;
	g_io_channel_write_chars(iochannelin, command, -1, &size, NULL);
	g_io_channel_flush(iochannelin, NULL);
	//printf("---> %s", command);
}

/*
void engine_restart()
{
	;
}
*/
void refresh_board()
{
	int i, j;
	for(i=0; i<boardsize; i++)
	{
		for(j=0; j<boardsize; j++)
		{
			if(board[i][j] == 0)
			{
				int f = 0;
				if(inforule == 2 && (computerside&1)==0 && piecenum%2==0 && forbid[i][j] && isgameover==0 && isthinking==0) f = 2;
				if(imgtypeboard[i][j] <= 8)
					gtk_image_set_from_pixbuf(GTK_IMAGE(imageboard[i][j]), pixbufboard[imgtypeboard[i][j]][max(0, f)]);
				else
					gtk_image_set_from_pixbuf(GTK_IMAGE(imageboard[i][j]), pixbufboard[0][max(1, f)]);
			}
			else
			{
				int f = 0;
				int x, y;
				if(movepath[piecenum-1]/boardsize == i && movepath[piecenum-1]%boardsize == j) f = 2;
				if(shownumber)
				{
					y = imgtypeboard[i][j]%9;
					x = 3+board[i][j]-1;
					if(pixbufboardnumber[y][x][boardnumber[i][j]][f?1:0] == NULL)
					{
						char n[10];
						sprintf(n, "%d", boardnumber[i][j]);
						if(f)
						{
							pixbufboardnumber[y][x][boardnumber[i][j]][1] = draw_overlay(pixbufboard[y][x], gdk_pixbuf_get_width(pixbufboard[y][x]), gdk_pixbuf_get_width(pixbufboard[y][x]), n, "#FF0000");
						}
						else
						{
							if(boardnumber[i][j] % 2 == 1)
								pixbufboardnumber[y][x][boardnumber[i][j]][0] = draw_overlay(pixbufboard[y][x], gdk_pixbuf_get_width(pixbufboard[y][x]), gdk_pixbuf_get_width(pixbufboard[y][x]), n, "#FFFFFF");
							else
								pixbufboardnumber[y][x][boardnumber[i][j]][0] = draw_overlay(pixbufboard[y][x], gdk_pixbuf_get_width(pixbufboard[y][x]), gdk_pixbuf_get_width(pixbufboard[y][x]), n, "#000000");
						}
					}
					gtk_image_set_from_pixbuf(GTK_IMAGE(imageboard[i][j]), pixbufboardnumber[y][x][boardnumber[i][j]][f?1:0]);
				}
				else
				{
					y = imgtypeboard[i][j]%9;
					x = 3+f+board[i][j]-1;
					gtk_image_set_from_pixbuf(GTK_IMAGE(imageboard[i][j]), pixbufboard[y][x]);
				}
			}
		}
	}
}
int is_legal_move(int y, int x)
{
	return y>=0 && x>=0 && y<boardsize && x<boardsize && board[y][x] == 0;
}
void make_move(int y, int x)
{
	int i, j, k;

	board[y][x] = piecenum%2+1;
	boardnumber[y][x] = piecenum+1;
	if(movepath[piecenum] != y*boardsize+x)
	{
		movepath[piecenum] = y*boardsize+x;
		for(i=piecenum+1; i<MAX_SIZE*MAX_SIZE; i++) movepath[i] = -1;
	}

	piecenum ++;
	if(piecenum == boardsize*boardsize) isgameover = 1;
	refresh_board();
	for(i=0; i<8; i+=2)
	{
		int ny, nx;
		k = 1;
		ny = y;
		nx = x;
		for(j=1; j<6; j++)
		{
			ny += movy[i];
			nx += movx[i];
			if(nx<0 || ny<0 || nx>=boardsize || ny>=boardsize) break;
			if(board[ny][nx] != board[y][x]) break;
			k ++;
		}
		ny = y;
		nx = x;
		for(j=1; j<6; j++)
		{
			ny -= movy[i];
			nx -= movx[i];
			if(nx<0 || ny<0 || nx>=boardsize || ny>=boardsize) break;
			if(board[ny][nx] != board[y][x]) break;
			k ++;
		}
		if(k==5 || (k>5 && inforule != 1))
		{
			isgameover = 1;
			break;
		}
	}
}
void show_forbid()
{
	int i;
	char command[80];
	if((computerside&1) || (piecenum%2))
	{
		memset(forbid, 0, sizeof(forbid));
		return;
	}
	sprintf(command, "start %d\n", boardsize);
	send_command(command);
	sprintf(command, "yxboard\n");
	send_command(command);
	for(i=0; i<piecenum; i++)
	{
		sprintf(command, "%d,%d,%d\n", movepath[i]/boardsize,
			movepath[i]%boardsize, piecenum%2==i%2 ? 1 : 2);
		send_command(command);
	}
	sprintf(command, "done\n");
	send_command(command);
	sprintf(command, "yxshowforbid\n");
	send_command(command);
}
int search_openbook(I64 zobkey)
{
	int l, r, m;
	static int flag = -1;
	if(flag != inforule+(specialrule<<2))
	{
		char name[80];
		FILE *in;
		openbooknum = 0;
		if(specialrule == 2)
		{
			sprintf(name, "book3_%d.dat", boardsize);
		}
		else
		{
			sprintf(name, "book%d_%d.dat", inforule, boardsize);
		}
		if((in = fopen(name, "rb")) != NULL)
		{
			while(!feof(in))
			{
				fread(&openbook[openbooknum].zobkey, sizeof(I64), 1, in);
				fread(&openbook[openbooknum].status, sizeof(char), 1, in);
				//printf("%I64d %c\n", openbook[openbooknum].zobkey, openbook[openbooknum].status);
				openbooknum ++;
			}
			fclose(in);
		}
		flag = inforule+(specialrule<<2);
	}
	//printf("%I64d\n", zobkey);
	if(openbooknum == 0) return 0;
	l = 0;
	r = openbooknum-1;
	while(l <= r)
	{
		m = (l+r)/2;
		if(zobkey >= openbook[m].zobkey)
			l = m + 1;
		else
			r = m - 1;
	}
	if(l>0 && openbook[l-1].zobkey == zobkey)
	{
		return openbook[l-1].status;
	}
	return 0;
}
int move_openbook(int *besty, int *bestx)
{
	int i, j, k;
	int p;
	static int flag = 1;
	static I64 zobrist[MAX_SIZE][MAX_SIZE][3];
	int result[MAX_SIZE][MAX_SIZE] = {{0}};
	int resultnum = 0;
	//FILE *out;
	//out = fopen("debug.txt", "w");

	if(flag)
	{
		FILE *in;
		if((in = fopen("base.dat", "r")) != NULL)
		{
			for(i=0; i<MAX_SIZE; i++)
			{
				for(j=0; j<MAX_SIZE; j++)
				{
					for(k=0; k<3; k++)
					{
						fscanf(in, "%I64d ", &zobrist[i][j][k]);
					}
				}
			}
			fclose(in);
			flag = 0;
		}
		else return 0;
	}
	/*
	for(i=0; i<piecenum; i++)
	{
		fprintf(out, "%d %d\n", movepath[i] / boardsize, movepath[i] % boardsize);
	}
	*/
	for(k=0; k<8; k++)
	{
		int _x, _y;
		I64 zobkey;
		I64 _zobkey;
		_zobkey = 0;
		for(i=0; i<piecenum; i++)
		{
			_y = movepath[i]/boardsize;
			_x = movepath[i]%boardsize;
			switch(k)
			{
				case 0: _zobkey ^= zobrist[_x][_y][i%2]; break;
				case 1: _zobkey ^= zobrist[_y][_x][i%2]; break;
				case 2: _zobkey ^= zobrist[boardsize-1-_x][_y][i%2]; break;
				case 3: _zobkey ^= zobrist[boardsize-1-_y][_x][i%2]; break;
				case 4: _zobkey ^= zobrist[_x][boardsize-1-_y][i%2]; break;
				case 5: _zobkey ^= zobrist[_y][boardsize-1-_x][i%2]; break;
				case 6: _zobkey ^= zobrist[boardsize-1-_x][boardsize-1-_y][i%2]; break;
				case 7: _zobkey ^= zobrist[boardsize-1-_y][boardsize-1-_x][i%2]; break;
			}
		}
		for(i=0; i<boardsize; i++)
		{
			for(j=0; j<boardsize; j++)
			{
				_y = i;
				_x = j;
				if(board[_y][_x] != 0 || result[_y][_x] != 0) continue;
				zobkey = _zobkey;
				switch(k)
				{
					case 0: zobkey ^= zobrist[_x][_y][piecenum%2]; break;
					case 1: zobkey ^= zobrist[_y][_x][piecenum%2]; break;
					case 2: zobkey ^= zobrist[boardsize-1-_x][_y][piecenum%2]; break;
					case 3: zobkey ^= zobrist[boardsize-1-_y][_x][piecenum%2]; break;
					case 4: zobkey ^= zobrist[_x][boardsize-1-_y][piecenum%2]; break;
					case 5: zobkey ^= zobrist[_y][boardsize-1-_x][piecenum%2]; break;
					case 6: zobkey ^= zobrist[boardsize-1-_x][boardsize-1-_y][piecenum%2]; break;
					case 7: zobkey ^= zobrist[boardsize-1-_y][boardsize-1-_x][piecenum%2]; break;
				}
				//fprintf(out, "%I64d\n", zobkey);
				if(search_openbook(zobkey) == 'a')
				{
					result[_y][_x] = 1;
					resultnum ++;
				}
			}
		}
	}
	//fclose(out);
	//printf("%d\n", search_openbook(-8487387668129966474ll));
	if(resultnum == 0) return 0;
	p = rand() % resultnum;
	k = 0;
	for(i=0; i<boardsize; i++)
	{
		for(j=0; j<boardsize; j++)
		{
			if(result[i][j])
			{
				if(p == k)
				{
					*besty = i;
					*bestx = j;
					return 1;
				}
				k ++;
			}
		}
	}
	return 1; //��ʵ�ϲ����ߵ���һ��
}

void show_dialog_swap_query(GtkWidget *window)
{
	GtkWidget *dialog;
	gint result;
	dialog = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, language==0?"Swap?":(language==1?_T("����?"):""));
	gtk_window_set_title(GTK_WINDOW(dialog), "Yixin");
	result = gtk_dialog_run(GTK_DIALOG(dialog));
	switch(result)
	{
		case GTK_RESPONSE_YES:
			if(specialrule == 2)
			{
				isneedrestart = 1;
				make_move(4, 5);
				//computerside ^= 3;
				if(computerside == 2)
				{
					change_side_menu(1, NULL);
					change_side_menu(-2, NULL);
				}
				else
				{
					change_side_menu(-1, NULL);
					change_side_menu(2, NULL);
				}
			}
			printf("swap\n");
			break;
		case GTK_RESPONSE_NO:
			printf("\n");
			break;
	}
	gtk_widget_destroy(dialog);
}

void show_dialog_swap_info(GtkWidget *window)
{
	GtkWidget *dialog;
	gint result;
	dialog = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_INFO, GTK_BUTTONS_OK, language==0?"Swap":(language==1?_T("����"):""));
	gtk_window_set_title(GTK_WINDOW(dialog), "Yixin");
	result = gtk_dialog_run(GTK_DIALOG(dialog));
	isneedrestart = 1;
	//computerside ^= 3;
	if(computerside == 2)
	{
		change_side_menu(1, NULL);
		change_side_menu(-2, NULL);
	}
	else
	{
		change_side_menu(-1, NULL);
		change_side_menu(2, NULL);
	}
	gtk_widget_destroy(dialog);
}

gboolean on_button_press_windowmain(GtkWidget *widget, GdkEventButton *event, GdkWindowEdge edge)
{
	int x, y;
	int size;
	char command[80];
	if(!isthinking && event->type == GDK_BUTTON_PRESS)
	{
		if(event->button == 1)
		{
			size = gdk_pixbuf_get_width(pixbufboard[0][0]);
			if(event->x - imageboard[0][0]->allocation.x < 0 || event->y - imageboard[0][0]->allocation.y < 0)
			{
				x = y = -1;
			}
			else
			{
				x = (int)((event->x - imageboard[0][0]->allocation.x)/size);
				y = (int)((event->y - imageboard[0][0]->allocation.y)/size);
			}
			if(x>=0 && x<boardsize && y>=0 && y<boardsize && !isgameover)
			{
				if(((computerside&1)&&piecenum%2==0) || ((computerside&2)&&piecenum%2==1))
				{
					int i;
					if(specialrule == 2 && piecenum == 0 && computerside != 3)
					{
						isneedrestart = 1;
						make_move(2, 3);
						show_dialog_swap_query(widget);
					}
					else if(useopenbook && move_openbook(&y, &x) && is_legal_move(y, x))
					{
						isneedrestart = 1;
						printf("use openbook\n");
						make_move(y, x);
						if(inforule == 2 && (computerside&1)==0)
						{
							sprintf(command, "yxshowforbid\n");
							send_command(command);
						}
					}
					else
					{
						isthinking = 1;
						isneedrestart = 0;
						timestart = clock();
						sprintf(command, "INFO time_left %d\n", timeoutmatch-timeused);
						send_command(command);
						sprintf(command, "start %d\n", boardsize);
						send_command(command);
						sprintf(command, "board\n");
						send_command(command);
						for(i=0; i<piecenum; i++)
						{
							sprintf(command, "%d,%d,%d\n", movepath[i]/boardsize,
								movepath[i]%boardsize, piecenum%2==i%2 ? 1 : 2);
							send_command(command);
						}
						sprintf(command, "done\n");
						send_command(command);
					}
				}
				else
				{
					if(board[y][x] == 0 && (piecenum%2==1 || !forbid[y][x]))
					{
						int i;
						make_move(y, x);
						if(specialrule == 2 && piecenum == 1 && computerside != 0)
						{
							int _x, _y;
							_x = min(x, boardsize-1-x);
							_y = min(y, boardsize-1-y);
							if((((_x==2&&_y==3)||(_x==3&&_y==2))&&rand()%2==1) || (_x>1 && _y>1 && _x+_y>5))
							{
								show_dialog_swap_info(widget);
							}
						}
						show_forbid();
						if(!isgameover && (((computerside&1)&&piecenum%2==0) || ((computerside&2)&&piecenum%2==1)))
						{
							if(useopenbook && move_openbook(&y, &x) && is_legal_move(y, x))
							{
								isneedrestart = 1;
								printf("use openbook\n");
								make_move(y, x);
								if(inforule == 2 && (computerside&1)==0)
								{
									sprintf(command, "yxshowforbid\n");
									send_command(command);
								}
							}
							else
							{
								if(isneedrestart)
								{
									isthinking = 1;
									isneedrestart = 0;
									timeused = 0;
									timestart = clock();
									sprintf(command, "INFO time_left %d\n", timeoutmatch-timeused);
									send_command(command);
									sprintf(command, "start %d\n", boardsize);
									send_command(command);
									sprintf(command, "board\n");
									send_command(command);
									for(i=0; i<piecenum; i++)
									{
										sprintf(command, "%d,%d,%d\n", movepath[i]/boardsize,
											movepath[i]%boardsize, piecenum%2==i%2 ? 1 : 2);
										send_command(command);
									}
									sprintf(command, "done\n");
									send_command(command);
								}
								else
								{
									sprintf(command, "INFO time_left %d\n", timeoutmatch-timeused);
									send_command(command);
									sprintf(command, "turn %d,%d\n", y, x);
									send_command(command);
									isthinking = 1;
									timestart = clock();
								}
							}
						}
					}
				}
				return FALSE;
			}
		}
	}
	return FALSE; /* Ϊʲô��FALSE? */
}

int is_integer(const char *str)
{
	while(*str)
	{
		if(*str < '0' || *str > '9') return 0;
		str ++;
	}
	return 1;
}

void set_level(int x)
{
	gchar command[80];
	levelchoice = x;
	if(levelchoice == 4)
	{
		sprintf(command, "INFO timeout_turn %d\n", timeoutturn);
		send_command(command);
		sprintf(command, "INFO timeout_match %d\n", timeoutmatch);
		send_command(command);
		sprintf(command, "INFO time_left %d\n", timeoutmatch-timeused);
		send_command(command);
		sprintf(command, "INFO max_node %d\n", 1000000000);
		send_command(command);
	}
	else
	{
		switch(levelchoice)
		{
			case 0:
				sprintf(command, "INFO max_node %d\n", 240000);
				send_command(command);
				break;
			case 1:
				sprintf(command, "INFO max_node %d\n", 120000);
				send_command(command);
				break;
			case 2:
				sprintf(command, "INFO max_node %d\n", 60000);
				send_command(command);
				break;
			case 3:
				sprintf(command, "INFO max_node %d\n", 30000); //if the speed is 500k, then it will take at most 60s
				send_command(command);
				break;
		}
		timeoutmatch = 1000000000;
		timeoutturn = 1000000000;
		sprintf(command, "INFO timeout_match %d\n", timeoutmatch);
		send_command(command);
		sprintf(command, "INFO time_left %d\n", timeoutmatch);
		send_command(command);
		sprintf(command, "INFO timeout_turn %d\n", timeoutturn);
		send_command(command);
	}
}

void show_dialog_settings(GtkWidget *widget, gpointer data)
{
	gchar command[80];
	gchar text[80];
	const gchar *ptext;
	GtkWidget *dialog;
	GtkWidget *hbox[2];
	GtkWidget *radiolevel[5];
	GtkWidget *labeltimeturn[2], *labeltimematch[2];
	GtkWidget *entrytimeturn, *entrytimematch;
	gint result;

	dialog = gtk_dialog_new_with_buttons("Settings", data, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "OK", 1, "Cancel", 2, NULL);
	gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	labeltimeturn[0] = gtk_label_new(language==0?"Turn:":(language==1?_T("��ʱ:"):""));
	entrytimeturn = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(entrytimeturn), 6);
	sprintf(text, "%d", timeoutturn/1000);
	gtk_entry_set_text(GTK_ENTRY(entrytimeturn), text);
	labeltimeturn[1] = gtk_label_new(language==0?"s  ":(language==1?_T("��  "):""));
	labeltimematch[0] = gtk_label_new(language==0?"Match:":(language==1?_T("��ʱ:"):""));
	entrytimematch =  gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(entrytimematch), 6);
	sprintf(text, "%d", timeoutmatch/1000);
	gtk_entry_set_text(GTK_ENTRY(entrytimematch), text);
	labeltimematch[1] = gtk_label_new(language==0?"s  ":(language==1?_T("��  "):""));

	radiolevel[0] = gtk_radio_button_new_with_label(NULL, language==0?"4 dan":(language==1?_T("ְҵ�Ķ�"):""));
	radiolevel[1] = gtk_radio_button_new_with_label(gtk_radio_button_get_group(GTK_RADIO_BUTTON(radiolevel[0])), language==0?"3 dan":(language==1?_T("ְҵ����"):""));
	radiolevel[2] = gtk_radio_button_new_with_label(gtk_radio_button_get_group(GTK_RADIO_BUTTON(radiolevel[1])), language==0?"2 dan":(language==1?_T("ְҵ����"):""));
	radiolevel[3] = gtk_radio_button_new_with_label(gtk_radio_button_get_group(GTK_RADIO_BUTTON(radiolevel[2])), language==0?"1 dan":(language==1?_T("ְҵһ��"):""));
	radiolevel[4] = gtk_radio_button_new_with_label(gtk_radio_button_get_group(GTK_RADIO_BUTTON(radiolevel[3])), language==0?"Custom Level":(language==1?_T("�Զ���"):""));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiolevel[levelchoice]),TRUE);

	hbox[0] = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox[0]), labeltimeturn[0], FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(hbox[0]), entrytimeturn, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(hbox[0]), labeltimeturn[1], FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(hbox[0]), labeltimematch[0], FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(hbox[0]), entrytimematch, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(hbox[0]), labeltimematch[1], FALSE, FALSE, 3);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), radiolevel[4], FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox[0], FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), radiolevel[0], FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), radiolevel[1], FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), radiolevel[2], FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), radiolevel[3], FALSE, FALSE, 3);
	
	gtk_widget_show_all(dialog);
	result = gtk_dialog_run(GTK_DIALOG(dialog));
	switch(result)
	{
		case 1:
			if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiolevel[4])))
			{
				ptext = gtk_entry_get_text(GTK_ENTRY(entrytimeturn));
				if(is_integer(ptext))
				{
					sscanf(ptext, "%d", &timeoutturn);
					if(timeoutturn > 1000000) timeoutturn = 1000000;
					if(timeoutturn < 1) timeoutturn = 1;
					timeoutturn *= 1000;
				}
				ptext = gtk_entry_get_text(GTK_ENTRY(entrytimematch));
				if(is_integer(ptext))
				{
					sscanf(ptext, "%d", &timeoutmatch);
					if(timeoutmatch > 1000000) timeoutmatch = 1000000;
					if(timeoutmatch < 1) timeoutmatch = 1;
					timeoutmatch *= 1000;
					if(timeoutmatch < timeoutturn) timeoutmatch = timeoutturn;
				}
				set_level(4);
				//sprintf(command, "INFO max_depth %d\n", boardsize * boardsize);
				//send_command(command);
			}
			else
			{
				if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiolevel[0])))
				{
					set_level(0);
				}
				else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiolevel[1])))
				{
					set_level(1);
				}
				else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiolevel[2])))
				{
					set_level(2);
				}
				else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiolevel[3])))
				{
					set_level(3);
				}
			}
			break;
		case 2:
			break;
	}
	gtk_widget_destroy(dialog);
}

void show_dialog_load(GtkWidget *widget, gpointer data)
{
	GtkWidget *dialog;
	GtkFileFilter* filter;
	FILE *in;
	dialog = gtk_file_chooser_dialog_new("Load", data, GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	//filter = gtk_file_filter_new();
	//gtk_file_filter_set_name(filter, "All files");
	//gtk_file_filter_add_pattern(filter, "*");
	//gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "Yixin saved positions");
	gtk_file_filter_add_pattern(filter, "*.[Ss][Aa][Vv]");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

	gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		char *filenameutf, *filename;
		filenameutf = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		filename = g_locale_from_utf8(filenameutf, -1, NULL, NULL, NULL);
		printf("%s\n", filename);
		if((in = fopen(filename, "r")) != NULL)
		{
			int i, num;
			new_game(NULL, NULL);
			fscanf(in, "%d", &boardsize);
			fscanf(in, "%*d");
			/*
			fscanf(in, "%d", &inforule);
			switch(inforule)
			{
				case 0: gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitemrule1), TRUE); break;
				case 1: gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitemrule2), TRUE); break;
				case 2: gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitemrule3), TRUE); break;
				case 3: gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitemrule4), TRUE); break;
			}
			*/
			change_rule(NULL, (gpointer)inforule);
			
			fscanf(in, "%d", &num);
			for(i=0; i<num; i++)
			{
				int x, y;
				fscanf(in, "%d %d", &x, &y);
				make_move(x, y);
			}
			fclose(in);
			show_forbid();
		}
		g_free(filenameutf);
		g_free(filename);
	}
	gtk_widget_destroy (dialog);
}
void show_dialog_save(GtkWidget *widget, gpointer data)
{
	GtkWidget *dialog;
	GtkFileFilter* filter;
	FILE *out;
	dialog = gtk_file_chooser_dialog_new("Save", data, GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "Yixin saved positions");
	gtk_file_filter_add_pattern(filter, "*.[Ss][Aa][Vv]");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		char *filenameutf, *filename;
		char _filename[256];
		int len;
		filenameutf = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		filename = g_locale_from_utf8(filenameutf, -1, NULL, NULL, NULL);
		len = strlen(filename);
		if(len>=4 && (filename[len-1]=='V' || filename[len-1]=='v') &&
			(filename[len-2]=='A' || filename[len-2]=='a') &&
			(filename[len-3]=='S' || filename[len-3]=='s') &&
			(filename[len-4]=='.'))
		{
			sprintf(_filename, "%s", filename);
		}
		else
		{
			sprintf(_filename, "%s.sav", filename);
		}
		printf("%s\n", _filename);
		if((out = fopen(_filename, "w")) != NULL)
		{
			int i;
			fprintf(out, "%d\n", boardsize);
			fprintf(out, "%d\n", inforule);
			fprintf(out, "%d\n", piecenum);
			for(i=0; i<piecenum; i++)
			{
				fprintf(out, "%d %d\n", movepath[i]/boardsize, movepath[i]%boardsize);
			}
			fclose(out);
		}
		g_free(filenameutf);
		g_free(filename);
	}
	gtk_widget_destroy (dialog);
}

void show_dialog_about(GtkWidget *widget, gpointer data)
{
	GtkWidget *dialog;
	GdkPixbuf *pixbuf;
	GtkWidget *icon;
	GtkWidget *name;
	GtkWidget *version;
	GtkWidget *author;
	GtkWidget *www;

	show_thanklist();

	dialog = gtk_dialog_new_with_buttons("About", data, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "OK", 1, NULL);
	gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	
	pixbuf = gdk_pixbuf_new_from_file("icon.ico", NULL);

	icon = gtk_image_new_from_pixbuf(pixbuf);
	g_object_unref(pixbuf);
	pixbuf = NULL;
	name = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(name), "<big><b>Yixin Board</b></big>");
	version = gtk_label_new("Version "VERSION);
	author = gtk_label_new("(C)2009-2013 Sun Kai");
	www = gtk_label_new("www.aiexp.info");
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), icon, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), name, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), version, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), author, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), www, FALSE, FALSE, 10);

	gtk_widget_show_all(dialog);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

void new_game(GtkWidget *widget, gpointer data)
{
	piecenum = 0;
	isgameover = 0;
	timeused = 0;
	memset(board, 0, sizeof(board));
	memset(forbid, 0, sizeof(forbid));
	refresh_board();
	if(isthinking) isneedomit ++;
	isthinking = 0;
	isneedrestart = 1;
}
void set_rule()
{
	char command[80];
	printf("INFO rule %d\n", inforule); fflush(stdout);
	sprintf(command, "INFO rule %d\n", inforule);
	send_command(command);
	isneedrestart = 1;
	show_forbid();
}
void change_rule(GtkWidget *widget, gpointer data)
{
	inforule = (int)data;
	if(inforule == 3)
	{
		inforule = 2;
		specialrule = 1;
	}
	else if(inforule == 4)
	{
		inforule = 0;
		specialrule = 2;
	}
	else
	{
		specialrule = 0;
	}
	set_rule();
}

void change_side(GtkWidget *widget, gpointer data)
{
	computerside ^= (int)data;
	isneedrestart = 1;
}
void change_side_menu(int flag, GtkWidget *w)
{
	static GtkWidget *rec[2];
	switch(flag)
	{
		case 3: rec[0] = w; break;
		case 4: rec[1] = w; break;
		case -1: gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(rec[0]), FALSE); break;
		case -2: gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(rec[1]), FALSE); break;
		case 1: gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(rec[0]), TRUE); break;
		case 2: gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(rec[1]), TRUE); break;
	}
}
void use_openbook(GtkWidget *widget, gpointer data)
{
	useopenbook ^= 1;
}
void view_numeration(GtkWidget *widget, gpointer data)
{
	shownumber ^= 1;
	refresh_board();
}
void change_piece(GtkWidget *widget, gpointer data)
{
	int i;
	int p = 0;

	switch((int)data)
	{
	case 0: p = 0; break;
	case 1: p = piecenum - 1; break;
	case 2: p = piecenum + 1; break;
	case 3: p = MAX_SIZE*MAX_SIZE; break;
	}
	while(p > 0 && movepath[p-1] == -1) p --;

	new_game(NULL, NULL);
	for(i=0; i<p; i++) make_move(movepath[i]/boardsize, movepath[i]%boardsize);
	show_forbid();
}
void stop_thinking(GtkWidget *widget, gpointer data)
{
	char command[80];
	sprintf(command, "yxstop\n");
	send_command(command);
}

void yixin_quit()
{
	FILE *out;
	if((out = fopen("settings.txt", "w")) != NULL)
	{
		fprintf(out, "%d\t;board size (10 ~ 20)\n", boardsize);
		fprintf(out, "%d\t;language (0: English, 1: Chinese)\n", language);
		fprintf(out, "%d\t;rule (0:freestyle, 1:standard, 2:free renju, 3:swap after 1st move)\n", specialrule==2?3:inforule);
		fprintf(out, "%d\t;openbook (0:not use, 1:use)\n", useopenbook);
		fprintf(out, "%d\t;computer play black (0:no, 1:yes)\n", computerside&1);
		fprintf(out, "%d\t;computer play white (0:no, 1:yes)\n", computerside>>1);
		fprintf(out, "%d\t;level(0: 4dan, 1:3dan, 2:2dan, 3:1dan, 4:customelevel)\n", levelchoice);
		fprintf(out, "%d\t;time limit(turn)\n", timeoutturn/1000);
		fprintf(out, "%d\t;time limit(match)\n", timeoutmatch/1000);
		fclose(out);
	}
	gtk_main_quit();
}

GdkPixbuf * copy_subpixbuf(GdkPixbuf *_src, int src_x, int src_y, int width, int height)
{
	GdkPixbuf *dst, *src;
	int sample, channels;
	gboolean alpha;
	GdkColorspace colorspace;
	
	guchar *pixels1, *pixels2;
	int rowstride1, rowstride2;
	int i, j;

	src = gdk_pixbuf_new_subpixbuf(_src, src_x, src_y, width, height);
	sample = gdk_pixbuf_get_bits_per_sample(src);
	channels = gdk_pixbuf_get_n_channels(src);
	alpha = gdk_pixbuf_get_has_alpha(src);
	colorspace = gdk_pixbuf_get_colorspace(src);
	dst = gdk_pixbuf_new(colorspace, alpha, sample, width, height);

	pixels1 = gdk_pixbuf_get_pixels(src);
	pixels2 = gdk_pixbuf_get_pixels(dst);

	rowstride1 = gdk_pixbuf_get_rowstride(src);
	rowstride2 = gdk_pixbuf_get_rowstride(dst);
	for(i=0; i<height; i++)
	{
		for(j=0; j<width; j++)
		{
			guchar *p1, *p2;
			p1 = pixels1 + i*rowstride1 + j*channels;
			p2 = pixels2 + i*rowstride2 + j*channels;
			memcpy(p2, p1, channels);
		}
	}
	g_object_unref(src);
	src = NULL;
	return dst;
}
void create_windowmain()
{
	GtkWidget *menubar, *menugame, *menuplayers, *menuview, *menuhelp, *menurule;
	GtkWidget *menuitemgame, *menuitemplayers, *menuitemview, *menuitemhelp;
	GtkWidget *menuitemnewgame, *menuitemload, *menuitemsave, *menuitemrule, *menuitemopenbook, *menuitemquit, *menuitemrule1, *menuitemrule2, *menuitemrule3, *menuitemrule4, *menuitemrule5, *menuitemnewrule[10];
	GtkWidget *menuitemcomputerplaysblack, *menuitemcomputerplayswhite, *menuitemsettings;
	GtkWidget *menuitemnumeration;
	GtkWidget *menuitemabout;

	GtkWidget *toolbar;
	GtkToolItem *toolgofirst, *toolgoback, *toolgoforward, *toolgolast, *toolstop;

	GdkPixbuf *pixbuf;

	GtkWidget *hbox[2];

	int size, sample, channels, rowstride;
	gboolean alpha;
	GdkColorspace colorspace;
	GdkPixbuf *background;

	int i, j, k, l;

	windowmain = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_resizable(GTK_WINDOW(windowmain), FALSE); /* ������� */
	g_signal_connect(GTK_OBJECT(windowmain), "destroy", G_CALLBACK(yixin_quit), NULL);
	gtk_widget_add_events(windowmain, GDK_BUTTON_PRESS_MASK); /* ���Ӱ�ť����¼� */
	gtk_signal_connect(GTK_OBJECT(windowmain), "button_press_event", G_CALLBACK(on_button_press_windowmain), NULL);
	gtk_window_set_title(GTK_WINDOW(windowmain), "Yixin");
	tableboard = gtk_table_new(boardsize+1, boardsize+1, TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(tableboard), 0); /* ��Ԫ�ؼ��Ϊ0 */
	gtk_table_set_col_spacings(GTK_TABLE(tableboard), 0); /* ��Ԫ�ؼ��Ϊ0 */

	pixbuf = gdk_pixbuf_new_from_file(piecepicname, NULL);
	size = gdk_pixbuf_get_height(pixbuf);
	sample = gdk_pixbuf_get_bits_per_sample(pixbuf);
	alpha = gdk_pixbuf_get_has_alpha(pixbuf);
	colorspace = gdk_pixbuf_get_colorspace(pixbuf);
	background = gdk_pixbuf_new_subpixbuf(pixbuf, size*9, 3, 1, 1);
	pixbufboard[0][0] = copy_subpixbuf(pixbuf, 0, 0, size, size);
	channels = gdk_pixbuf_get_n_channels(pixbufboard[0][0]);
	rowstride = gdk_pixbuf_get_rowstride(pixbufboard[0][0]);

	pixbufboard[1][0] = copy_subpixbuf(pixbuf, size, 0, size, size);
	for(i=2; i<=4; i++)
	{
		guchar *pixels1, *pixels2;

		pixbufboard[i][0] = gdk_pixbuf_new(colorspace, alpha, sample, size, size);
		pixels1 = gdk_pixbuf_get_pixels(pixbufboard[i-1][0]);
		pixels2 = gdk_pixbuf_get_pixels(pixbufboard[i][0]);
		for(j=0; j<size; j++)
		{
			for(k=0; k<size; k++)
			{
				guchar *p1, *p2;
				p1 = pixels1 + j*rowstride + k*channels;
				p2 = pixels2 + k*rowstride + (size-1-j)*channels;
				memcpy(p2, p1, channels);
			}
		}
	}
	pixbufboard[5][0] = copy_subpixbuf(pixbuf, size*2, 0, size, size);
	for(i=6; i<=8; i++)
	{
		guchar *pixels1, *pixels2, *pixels3;

		pixbufboard[i][0] = gdk_pixbuf_new(colorspace, alpha, sample, size, size);
		pixels1 = gdk_pixbuf_get_pixels(pixbufboard[i-1][0]);
		pixels2 = gdk_pixbuf_get_pixels(pixbufboard[i][0]);
		pixels3 = gdk_pixbuf_get_pixels(background);
		for(j=0; j<size; j++)
		{
			for(k=0; k<size; k++)
			{
				guchar *p1, *p2;
				if(size%2 == 1 || i!=7)
				{
					p1 = pixels1 + j*rowstride + k*channels;
					p2 = pixels2 + k*rowstride + (size-1-j)*channels;
				}
				else
				{
					p1 = pixels1 + ((j+1)%size)*rowstride + k*channels;
					p2 = pixels2 + k*rowstride + (size-1-j)*channels;
				}
				memcpy(p2, p1, channels);
			}
		}
	}
	for(i=0; i<=8; i++)
	{
		for(j=1; j<=2; j++)
		{
			GdkPixbuf *pbt;
			guchar *pixels1, *pixels2, *pixels3, *pixels4;
			pbt = copy_subpixbuf(pixbuf, size*(j+2), 0, size, size);
			pixbufboard[i][j] = gdk_pixbuf_new(colorspace, alpha, sample, size, size);
			pixels1 = gdk_pixbuf_get_pixels(pixbufboard[i][0]);
			pixels2 = gdk_pixbuf_get_pixels(pbt);
			pixels3 = gdk_pixbuf_get_pixels(background);
			pixels4 = gdk_pixbuf_get_pixels(pixbufboard[i][j]);
			for(k=0; k<size; k++)
			{
				for(l=0; l<size; l++)
				{
					guchar *p1, *p2, *p3, *p4;
					p1 = pixels1 + k*rowstride + l*channels;
					p2 = pixels2 + k*rowstride + l*channels;
					p3 = pixels3;
					p4 = pixels4 + k*rowstride + l*channels;
					if(memcmp(p2, p3, channels) != 0)
						memcpy(p4, p2, channels);
					else
						memcpy(p4, p1, channels);
				}
			}
			g_object_unref(pbt);
			pbt = NULL;
		}
		for(j=3; j<=4; j++)
		{
			GdkPixbuf *pbt1, *pbt2;
			guchar *pixels1, *pixels2, *pixels3, *pixels4, *pixels5;
			pbt1 = copy_subpixbuf(pixbuf, size*(j+4), 0, size, size);
			pbt2 = copy_subpixbuf(pixbuf, size*6, 0, size, size);
			pixbufboard[i][j] = gdk_pixbuf_new(colorspace, alpha, sample, size, size);
			pixels1 = gdk_pixbuf_get_pixels(pixbufboard[i][0]);
			pixels2 = gdk_pixbuf_get_pixels(pbt1);
			pixels3 = gdk_pixbuf_get_pixels(pbt2);
			pixels4 = gdk_pixbuf_get_pixels(background);
			pixels5 = gdk_pixbuf_get_pixels(pixbufboard[i][j]);
			for(k=0; k<size; k++)
			{
				for(l=0; l<size; l++)
				{
					guchar *p1, *p2, *p3, *p4, *p5;
					p1 = pixels1 + k*rowstride + l*channels;
					p2 = pixels2 + k*rowstride + l*channels;
					p3 = pixels3 + k*rowstride + l*channels;
					p4 = pixels4;
					p5 = pixels5 + k*rowstride + l*channels;
					if(memcmp(p3, p4, channels) != 0 || memcmp(p1, p4, channels) == 0)
						memcpy(p5, p2, channels);
					else
						memcpy(p5, p1, channels);
				}
			}
			g_object_unref(pbt1);
			pbt1 = NULL;
			g_object_unref(pbt2);
			pbt2 = NULL;
		}
		for(j=5; j<=6; j++)
		{
			GdkPixbuf *pbt;
			guchar *pixels1, *pixels2, *pixels3, *pixels4;
			pbt = copy_subpixbuf(pixbuf, size*5, 0, size, size);
			pixbufboard[i][j] = gdk_pixbuf_new(colorspace, alpha, sample, size, size);
			pixels1 = gdk_pixbuf_get_pixels(pixbufboard[i][j-2]);
			pixels2 = gdk_pixbuf_get_pixels(pbt);
			pixels3 = gdk_pixbuf_get_pixels(background);
			pixels4 = gdk_pixbuf_get_pixels(pixbufboard[i][j]);
			for(k=0; k<size; k++)
			{
				for(l=0; l<size; l++)
				{
					guchar *p1, *p2, *p3, *p4;
					p1 = pixels1 + k*rowstride + l*channels;
					p2 = pixels2 + k*rowstride + l*channels;
					p3 = pixels3;
					p4 = pixels4 + k*rowstride + l*channels;
					if(memcmp(p2, p3, channels) != 0)
						memcpy(p4, p2, channels);
					else
						memcpy(p4, p1, channels);
				}
			}
			g_object_unref(pbt);
			pbt = NULL;
		}
	}
	
	for(i=0; i<boardsize; i++)
	{
		for(j=0; j<boardsize; j++)
		{
			if(i==0 && j==0) imgtypeboard[i][j] = 1;
			else if(i==0 && j==boardsize-1) imgtypeboard[i][j] = 2;
			else if(i==boardsize-1 && j==boardsize-1) imgtypeboard[i][j] = 3;
			else if(i==boardsize-1 && j==0) imgtypeboard[i][j] = 4;
			else if(i==0) imgtypeboard[i][j] = 5;
			else if(j==boardsize-1) imgtypeboard[i][j] = 6;
			else if(i==boardsize-1) imgtypeboard[i][j] = 7;
			else if(j==0) imgtypeboard[i][j] = 8;
			else if(i==boardsize/2 && j==boardsize/2) imgtypeboard[i][j] = 9;
			else if(i==boardsize/4 && j==boardsize/4) imgtypeboard[i][j] = 9;
			else if(i==boardsize/4 && j==boardsize-1-boardsize/4) imgtypeboard[i][j] = 9;
			else if(i==boardsize-1-boardsize/4 && j==boardsize/4) imgtypeboard[i][j] = 9;
			else if(i==boardsize-1-boardsize/4 && j==boardsize-1-boardsize/4) imgtypeboard[i][j] = 9;
			else imgtypeboard[i][j] = 0;
		}
	}
	for(i=0; i<boardsize; i++)
	{
		char tlabel[3];
		sprintf(tlabel, "%d", boardsize-i);
		labelboard[0][i] = gtk_label_new(tlabel);
		gtk_table_attach_defaults(GTK_TABLE(tableboard), labelboard[0][i], boardsize, boardsize+1, i, i+1);
		sprintf(tlabel, "%c", 'A'+i);
		labelboard[1][i] = gtk_label_new(tlabel);
		gtk_table_attach_defaults(GTK_TABLE(tableboard), labelboard[1][i], i, i+1, boardsize, boardsize+1);
	}
	for(i=0; i<boardsize; i++)
	{
		for(j=0; j<boardsize; j++)
		{
			if(imgtypeboard[i][j] <= 8)
				imageboard[i][j] = gtk_image_new_from_pixbuf(pixbufboard[imgtypeboard[i][j]][0]);
			else
				imageboard[i][j] = gtk_image_new_from_pixbuf(pixbufboard[0][1]);
			gtk_table_attach_defaults(GTK_TABLE(tableboard), imageboard[i][j], j, j+1, i, i+1);
		}
	}

	g_object_unref(background);
	background = NULL;
	g_object_unref(pixbuf);
	pixbuf = NULL;

	menubar = gtk_menu_bar_new();
	menugame = gtk_menu_new();
	menuplayers = gtk_menu_new();
	menuview = gtk_menu_new();
	menuhelp = gtk_menu_new();
	menurule = gtk_menu_new();

	menuitemgame = gtk_menu_item_new_with_label(language==0?"Game":(language==1?_T("��Ϸ"):""));
	menuitemplayers = gtk_menu_item_new_with_label(language==0?"Players":(language==1?_T("����"):""));
	menuitemview = gtk_menu_item_new_with_label(language==0?"View":(language==1?_T("��ʾ"):""));
	menuitemhelp = gtk_menu_item_new_with_label(language==0?"Help":(language==1?_T("����"):""));
	menuitemnewgame = gtk_menu_item_new_with_label(language==0?"New":(language==1?_T("�½�"):""));
	menuitemload = gtk_menu_item_new_with_label(language==0?"Load":(language==1?_T("����"):""));
	menuitemsave = gtk_menu_item_new_with_label(language==0?"Save":(language==1?_T("����"):""));
	menuitemrule = gtk_menu_item_new_with_label(language==0?"Rule":(language==1?_T("����"):""));
	menuitemopenbook = gtk_check_menu_item_new_with_label(language==0?"Use Openbook":(language==1?_T("ʹ�ÿ��ֿ�"):""));
	menuitemnumeration = gtk_check_menu_item_new_with_label(language==0?"Numeration":(language==1?_T("��ʾ����"):""));
	menuitemquit = gtk_menu_item_new_with_label(language==0?"Quit":(language==1?_T("�˳�"):""));
	menuitemabout = gtk_menu_item_new_with_label(language==0?"About":(language==1?_T("����"):""));
	menuitemsettings = gtk_menu_item_new_with_label(language==0?"Settings":(language==1?_T("����"):""));
	menuitemrule1 = gtk_radio_menu_item_new_with_label(NULL, language==0?"Freestyle gomoku":(language==1?_T("�޽���"):""));
	menuitemrule2 = gtk_radio_menu_item_new_with_label(gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitemrule1)), language==0?"Standrad gomoku":(language==1?_T("�޽���(����Ӯ)"):""));
	menuitemrule3 = gtk_radio_menu_item_new_with_label(gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitemrule1)), language==0?"Free renju":(language==1?_T("�н���"):""));
	//menuitemrule4 = gtk_radio_menu_item_new_with_label(gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitemrule1)), language==0?"Professional renju":(language==1?_T("RIF����"):""));
	menuitemrule5 = gtk_radio_menu_item_new_with_label(gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitemrule1)), language==0?"Swap after 1st move":(language==1?_T("һ�ֽ���"):""));
	
	switch(inforule)
	{
		case 0:
			if(specialrule == 0)
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitemrule1), TRUE);
			else if(specialrule == 2)
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitemrule5), TRUE);
			break;
		case 1: gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitemrule2), TRUE); break;
		case 2: gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitemrule3), TRUE); break;
	}
	set_rule();

	if(useopenbook)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitemopenbook), TRUE);
	}
	if(shownumber)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitemnumeration), TRUE);
	}
	menuitemcomputerplaysblack = gtk_check_menu_item_new_with_label(language==0?"Computer plays black":(language==1?_T("�����ִ��"):""));
	menuitemcomputerplayswhite = gtk_check_menu_item_new_with_label(language==0?"Computer plays white":(language==1?_T("�����ִ��"):""));
	if(computerside&1)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitemcomputerplaysblack), TRUE);
	else
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitemcomputerplaysblack), FALSE);
	if(computerside>>1)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitemcomputerplayswhite), TRUE);
	else
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitemcomputerplayswhite), FALSE);
	change_side_menu(3, menuitemcomputerplaysblack);
	change_side_menu(4, menuitemcomputerplayswhite);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitemgame), menugame);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitemplayers), menuplayers);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitemview), menuview);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitemhelp), menuhelp);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitemrule), menurule);

	gtk_menu_shell_append(GTK_MENU_SHELL(menurule), menuitemrule1);
	gtk_menu_shell_append(GTK_MENU_SHELL(menurule), menuitemrule2);
	gtk_menu_shell_append(GTK_MENU_SHELL(menurule), menuitemrule3);
	//gtk_menu_shell_append(GTK_MENU_SHELL(menurule), menuitemrule4);
	gtk_menu_shell_append(GTK_MENU_SHELL(menurule), menuitemrule5);

	gtk_menu_shell_append(GTK_MENU_SHELL(menugame), menuitemnewgame);
	gtk_menu_shell_append(GTK_MENU_SHELL(menugame), menuitemload);
	gtk_menu_shell_append(GTK_MENU_SHELL(menugame), menuitemsave);
	gtk_menu_shell_append(GTK_MENU_SHELL(menugame), menuitemrule);
	gtk_menu_shell_append(GTK_MENU_SHELL(menugame), menuitemopenbook);
	gtk_menu_shell_append(GTK_MENU_SHELL(menugame), menuitemquit);
	gtk_menu_shell_append(GTK_MENU_SHELL(menuview), menuitemnumeration);
	gtk_menu_shell_append(GTK_MENU_SHELL(menuplayers), menuitemcomputerplaysblack);
	gtk_menu_shell_append(GTK_MENU_SHELL(menuplayers), menuitemcomputerplayswhite);
	gtk_menu_shell_append(GTK_MENU_SHELL(menuplayers), menuitemsettings);
	gtk_menu_shell_append(GTK_MENU_SHELL(menuhelp), menuitemabout);

	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menuitemgame);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menuitemplayers);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menuitemview);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menuitemhelp);

	g_signal_connect(G_OBJECT(menuitemnewgame), "activate", G_CALLBACK(new_game), NULL);
	g_signal_connect(G_OBJECT(menuitemload), "activate", G_CALLBACK(show_dialog_load), windowmain);
	g_signal_connect(G_OBJECT(menuitemsave), "activate", G_CALLBACK(show_dialog_save), windowmain);
	g_signal_connect(G_OBJECT(menuitemopenbook), "activate", G_CALLBACK(use_openbook), NULL);
	g_signal_connect(G_OBJECT(menuitemnumeration), "activate", G_CALLBACK(view_numeration), NULL);
	g_signal_connect(G_OBJECT(menuitemquit), "activate", G_CALLBACK(yixin_quit), NULL);
	g_signal_connect(G_OBJECT(menuitemabout), "activate", G_CALLBACK(show_dialog_about), GTK_WINDOW(windowmain));
	g_signal_connect(G_OBJECT(menuitemsettings), "activate", G_CALLBACK(show_dialog_settings), GTK_WINDOW(windowmain));
	g_signal_connect(G_OBJECT(menuitemrule1), "activate", G_CALLBACK(change_rule), (gpointer)0);
	g_signal_connect(G_OBJECT(menuitemrule2), "activate", G_CALLBACK(change_rule), (gpointer)1);
	g_signal_connect(G_OBJECT(menuitemrule3), "activate", G_CALLBACK(change_rule), (gpointer)2);
	//g_signal_connect(G_OBJECT(menuitemrule4), "activate", G_CALLBACK(change_rule), (gpointer)3);
	g_signal_connect(G_OBJECT(menuitemrule5), "activate", G_CALLBACK(change_rule), (gpointer)4);
	g_signal_connect(G_OBJECT(menuitemcomputerplaysblack), "activate", G_CALLBACK(change_side), (gpointer)1);
	g_signal_connect(G_OBJECT(menuitemcomputerplayswhite), "activate", G_CALLBACK(change_side), (gpointer)2);

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
	//gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);
	gtk_toolbar_set_orientation((GtkToolbar*)toolbar, GTK_ORIENTATION_VERTICAL);
	toolgofirst = gtk_tool_button_new_from_stock(GTK_STOCK_GOTO_FIRST);
	toolgoback = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
	toolgoforward = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
	toolgolast = gtk_tool_button_new_from_stock(GTK_STOCK_GOTO_LAST);
	toolstop = gtk_tool_button_new_from_stock(GTK_STOCK_STOP);
	
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolgofirst, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolgoback, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolgoforward, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolgolast, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolstop, -1);

	g_signal_connect(G_OBJECT(toolgofirst), "clicked", G_CALLBACK(change_piece), (gpointer)0);
	g_signal_connect(G_OBJECT(toolgoback), "clicked", G_CALLBACK(change_piece), (gpointer)1);
	g_signal_connect(G_OBJECT(toolgoforward), "clicked", G_CALLBACK(change_piece), (gpointer)2);
	g_signal_connect(G_OBJECT(toolgolast), "clicked", G_CALLBACK(change_piece), (gpointer)3);
	g_signal_connect(G_OBJECT(toolstop), "clicked", G_CALLBACK(stop_thinking), NULL);

	hbox[0] = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox[0]), toolbar, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(hbox[0]), tableboard, FALSE, FALSE, 3);

	vboxwindowmain = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vboxwindowmain), menubar, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vboxwindowmain), hbox[0], FALSE, FALSE, 3);
	//gtk_box_pack_start(GTK_BOX(vboxwindowmain), toolbar, FALSE, FALSE, 3);
	//gtk_box_pack_start(GTK_BOX(vboxwindowmain), tableboard, FALSE, FALSE, 3);

	gtk_container_add(GTK_CONTAINER(windowmain), vboxwindowmain);
	gtk_widget_show_all(windowmain);
}

gboolean iochannelout_watch(GIOChannel *channel, GIOCondition cond, gpointer data)
{
	gchar *string;
	gsize size;
	int x, y;
	int i;
	char command[80];

	if(cond & G_IO_HUP)
	{
		g_io_channel_unref(channel);
		return FALSE;
	}
	do
	{
		g_io_channel_read_line(channel, &string, &size, NULL, NULL);
		for(i=0; i<(int)size; i++)
		{
			if(string[i] >= 'a' && string[i] <= 'z') string[i] = string[i] - 'a' + 'A';
		}
		printf("%s", string);
		if(memcmp(string, "OK", 2) == 0)
		{
			g_free(string);
			continue;
		}
		if(memcmp(string, "MESSAGE", 7) == 0)
		{
			g_free(string);
			continue;
		}
		if(memcmp(string, "DETAIL", 6) == 0)
		{
			g_free(string);
			continue;
		}
		if(memcmp(string, "DEBUG", 5) == 0)
		{
			g_free(string);
			continue;
		}
		if(memcmp(string, "ERROR", 5) == 0)
		{
			g_free(string);
			continue;
		}
		if(memcmp(string, "UNKNOWN", 7) == 0)
		{
			g_free(string);
			continue;
		}
		if(memcmp(string, "FORBID", 6) == 0)
		{
			memset(forbid, 0, sizeof(forbid));
			for(i=7; string[i] != '.'; i+=4)
			{
				y = (string[i] - '0')*10 + string[i+1] - '0';
				x = (string[i+2] - '0')*10 + string[i+3] - '0';
				forbid[y][x] = 1;
			}
			g_free(string);
			refresh_board();
			continue;
		}
		sscanf(string, "%d,%d", &y, &x);
		if(isneedomit > 0)
		{
			g_free(string);
			isneedomit --;
		}
		else
		{
			isthinking = 0;
			timeused += clock() - timestart;
			printf("match time left: %dms\n", timeoutmatch-timeused);
			if(is_legal_move(y, x))
			{
				make_move(y, x);
			}
			else
			{
				isgameover = 1;
			}
			if(inforule == 2 && (computerside&1)==0)
			{
				sprintf(command, "yxshowforbid\n");
				send_command(command);
			}
			g_free(string);
			if(!isgameover && (((computerside&1)&&piecenum%2==0) || ((computerside&2)&&piecenum%2==1)))
			{
				isthinking = 1;
				timeused = 0;
				timestart = clock();
				sprintf(command, "INFO time_left %d\n", timeoutmatch-timeused);
				send_command(command);
				sprintf(command, "start %d\n", boardsize);
				send_command(command);
				sprintf(command, "board\n");
				send_command(command);
				for(i=0; i<piecenum; i++)
				{
					sprintf(command, "%d,%d,%d\n", movepath[i]/boardsize,
						movepath[i]%boardsize, piecenum%2==i%2 ? 1 : 2);
					send_command(command);
				}
				sprintf(command, "done\n");
				send_command(command);
			}
		}
	} while(g_io_channel_get_buffer_condition(channel) == G_IO_IN);
	return TRUE; /* TRUE��ʾ�ص�������֮����Ȼ������ת��FALSE����ִ�� */
}
gboolean iochannelerr_watch(GIOChannel *channel, GIOCondition cond, gpointer data)
{
	if(cond & G_IO_HUP)
	{
		g_io_channel_unref(channel);
		return FALSE ;
	}
	return TRUE;
}
int read_int_from_file(FILE *in)
{
	int flag = 0;
	int num = 0;
	char c;

	while(fscanf(in, "%c", &c) != EOF)
	{
		if(flag&1)
		{
			if(c=='\n') flag &= ~1;
		}
		else
		{
			if(c==';')
			{
				flag |= 1;
				if(flag & 2) return (flag&4)?-num:num;
			}
			else if(c<='9' && c>='0')
			{
				num *= 10;
				num += c-'0';
				flag |= 2;
			}
			else if(c=='-')
			{
				num = 0;
				flag |= 2;
				flag |= 4;
			}
			else
			{
				if(flag & 2) return (flag&4)?-num:num;
			}
		}
	}
	return (flag&4)?-num:num;
}
void load_setting()
{
	FILE *in;
	char s[80];
	int t;
	if((in = fopen("settings.txt", "r")) != NULL)
	{
		boardsize = read_int_from_file(in);
		if(boardsize > MAX_SIZE || boardsize < 5) boardsize = 15;
		language = read_int_from_file(in);
		if(language < 0 || language > 1) language = 0;
		inforule = read_int_from_file(in);
		if(inforule < 0 || inforule > 3) inforule = 0;
		if(inforule == 3)
		{
			inforule = 0;
			specialrule = 2;
		}
		useopenbook = read_int_from_file(in);
		if(useopenbook < 0 || useopenbook > 1) useopenbook = 1;
		computerside = 0;
		t = read_int_from_file(in);
		if(t == 1) computerside |= 1;
		t = read_int_from_file(in);
		if(t == 1) computerside |= 2;
		levelchoice = read_int_from_file(in);
		if(levelchoice < 0 || levelchoice > 4) levelchoice = 4;
		timeoutturn = read_int_from_file(in) * 1000;
		if(timeoutturn <= 0 || timeoutturn > 1000000000) timeoutturn = 5000;
		timeoutmatch = read_int_from_file(in) * 1000;
		if(timeoutmatch <= 0 || timeoutmatch > 1000000000) timeoutmatch = 1000000;
		fclose(in);
	}
	sprintf(s, "piece_%d.bmp", boardsize);
	if((in = fopen(s, "rb")) != NULL)
	{
		strcpy(piecepicname, s);
		fclose(in);
	}
	piecenum = 0;
	memset(movepath, -1, sizeof(movepath));
}
void load_engine()
{
#ifdef G_OS_WIN32
	gchar *argv[] = {"engine.exe", NULL};
#else
	gchar *argv[] = {"./engine", NULL};
#endif
	GPid pid;
	gint in, out, err;
	gboolean ret;
	GError *error = NULL;

	ret = g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
		NULL, NULL, &pid, &in, &out, &err, &error);
	if(!ret)
	{
		FILE *out;
		out = fopen("ERROR.txt", "w");
		fprintf(out, "%s\n", error->message);
		fclose(out);
		g_error_free(error);

		g_error("Cannot load engine!");
		return;
	}
#ifdef G_OS_WIN32
	iochannelin = g_io_channel_win32_new_fd(in);
    iochannelout = g_io_channel_win32_new_fd(out);
    iochannelerr = g_io_channel_win32_new_fd(err);
#else
	iochannelin = g_io_channel_unix_new(in);
    iochannelout = g_io_channel_unix_new(out);
    iochannelerr = g_io_channel_unix_new(err);
#endif
	g_io_add_watch(iochannelout, G_IO_IN | G_IO_PRI | G_IO_HUP, (GIOFunc)iochannelout_watch, NULL);
	//g_io_add_watch_full(iochannelout, G_PRIORITY_HIGH, G_IO_IN | G_IO_HUP, (GIOFunc)iochannelout_watch, NULL, NULL);
    g_io_add_watch(iochannelerr, G_IO_IN | G_IO_PRI | G_IO_HUP, (GIOFunc)iochannelerr_watch, NULL);
}
void init_engine()
{
	char command[80];
	sprintf(command, "START %d\n", boardsize);
	send_command(command);
	set_level(levelchoice);
}
int main(int argc, char** argv)  
{
	gtk_init(&argc, &argv);
	srand((unsigned)time(NULL));
	load_setting();
	load_engine();
	init_engine();
	gtk_window_set_default_icon(gdk_pixbuf_new_from_file("icon.ico", NULL)); /* ���д���Ĭ��ͼ�� */
	create_windowmain();
	gtk_main();
	return 0;
}