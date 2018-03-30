/*
 * Copyright (C) 2012 Mark Hills <mark@pogo.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */


/*
 *  Copyright (C) 2004 Steve Harris, Uwe Koloska
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2.1 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  $Id$
 */

#define _GNU_SOURCE /* strdupa() */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "osc.h"
#include "player.h"
#include "deck.h"
#include "track.h"
#include "cues.h"
#include "player.h"

#include "lo/lo.h"

lo_server_thread st;
lo_server_thread st_tcp;
pthread_t thread_osc_updater;
struct deck *osc_deck;
struct library *osc_library;
int osc_ndeck = 0;
lo_address address[2];
int osc_nconnection = 0;
int osc_nclient = 0;


/*
 * Start osc server and add methods
 */

int osc_start(struct deck *deck, struct library *library, size_t ndeck)
{
    osc_deck = deck;
    osc_library = library;
    osc_ndeck = ndeck;

    /* why do we have two addresses here? They're both the same! */
    address[0] = lo_address_new_from_url("osc.udp://0.0.0.0:7771/");
    address[1] = lo_address_new_from_url("osc.udp://0.0.0.0:7771/");

    /* start a new server on port 7770 */
    st = lo_server_thread_new("7770", error);

    lo_server_thread_add_method(st, "/xwax/load_track", "isssd", load_track_handler, NULL);

    lo_server_thread_add_method(st, "/xwax/get_status", "i", get_status_handler, NULL);

    lo_server_thread_add_method(st, "/xwax/get_monitor", "i", get_monitor_handler, NULL);

    lo_server_thread_add_method(st, "/xwax/recue", "i", recue_handler, NULL);

    lo_server_thread_add_method(st, "/xwax/disconnect", "i", disconnect_handler, NULL);

    lo_server_thread_add_method(st, "/xwax/reconnect", "i", reconnect_handler, NULL);

    lo_server_thread_add_method(st, "/xwax/quit", "", quit_handler, NULL);

    lo_server_thread_start(st);

    return 0;
}

/*
 * Stop all osc services
 */
void osc_stop()
{
    lo_server_thread_free(st);
}

/*
 * print error message about osc related things
 */

void error(int num, const char *msg, const char *path)
{
    fprintf(stderr, "liblo server error %d in path %s: %s\n", num, path, msg);
    fflush(stderr);
}

/*
 * catch any incoming messages and display them. returning 1 means that the
 * message has not been fully handled and the server should try other methods
 */

int generic_handler(const char *path, const char *types, lo_arg ** argv,
                    int argc, void *data, void *user_data)
{
    int i;

    fprintf(stderr, "Unsupported OSC message. path: <%s>\n", path);
    for (i = 0; i < argc; i++) {
        fprintf(stderr, "arg %d '%c' ", i, types[i]);
        lo_arg_pp((lo_type)types[i], argv[i]);
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");
    fflush(stderr);

    return 1;
}

/*
 * callback function for /xwax/load_trackk
 */

int load_track_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    /* example showing pulling the argument values out of the argv array */
    fprintf(stderr, "%s <- deck:%i path:%s artist:%s title:%s bpm:%g\n",
            path, argv[0]->i, &argv[1]->s, &argv[2]->s, &argv[3]->s, argv[4]->d);
    fflush(stderr);

    int d, i;
    struct deck *de;
    struct record *r;
	struct listing storage;
	bool success = false;
    char * pathname = strdup(&argv[1]->s);

    d = argv[0]->i;
    if (d >= osc_ndeck) {
        fprintf(stderr, "Trying to access deck %d\n  osc_ndeck = %d\n", d, osc_ndeck);
        error(255, path, "Trying to access into invalid deck");
        return 255;
    }
    de = &osc_deck[d];

	/*
    r = malloc(sizeof *r);
    if (r == NULL) {
        perror("malloc");
        return -1;
    }

	// One could try to find the record by pathname and only load if it already exists library->storage->by_artist->record->pathname
	// Loading the entire library doesn't take long and this would fix the obvious memory leak
	// BUT: the problem typically seems to come up when trying to access a "track" rather than a "record"
	// track contains more audio like information
    r->pathname = strdup(&argv[1]->s);
    r->artist = strdup(&argv[2]->s);
    r->title = strdup(&argv[3]->s);
    r->bpm = (double) argv[4]->d;

    r = library_add(osc_library, r);
    if (r == NULL) {
        // FIXME: memory leak, need to do record_clear(r)
        return -1;
    }
	*/


	storage = osc_library->storage;
	for (i=0; i<storage.by_artist.entries; i++) {
		r = storage.by_artist.record[i];
		if (strcmp(pathname, r->pathname) == 0) {
			success = true;
			break;
		}
	}

	if (r == NULL) {
		return -1;
	}
	if (success) {
		deck_load(&osc_deck[d], r);
	} else {
		fprintf(stderr, "Error loading path %s", pathname);
	}


    return 0;
}

/*
 * replies to get_status request calls by sending the status back
 */

int get_status_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    /* example showing pulling the argument values out of the argv array */
    printf("%s <- deck:%i\n", path, argv[0]->i);
    fflush(stdout);

    int d = argv[0]->i;

    if (d >= osc_ndeck) {
        error(255, path, "Trying to access into invalid deck");
        return 255;
    }

    // lo_address a = lo_message_get_source(data);
    lo_address a = lo_address_new("0.0.0.0", "7771");
    printf("PORT: %s\n", lo_address_get_port(a));

    char* url = lo_address_get_url(a);

    printf("URL: %s\n", url);

    osc_send_status(a, d);

    return 0;
}


/*
 * replies to get_monitor request calls by sending the status back
 */


int get_monitor_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    /* example showing pulling the argument values out of the argv array */
    printf("%s <- deck:%i\n", path, argv[0]->i);
    fflush(stdout);

    int d = argv[0]->i;

    if (d >= osc_ndeck) {
        error(255, path, "Trying to access into invalid deck");
        return 255;
    }

    // lo_address a = lo_message_get_source(data);
    lo_address a = lo_address_new("0.0.0.0", "7771");
    printf("PORT: %s\n", lo_address_get_port(a));

    char* url = lo_address_get_url(a);

    printf("URL: %s\n", url);

    osc_send_monitor(a, d);

    return 0;
}

/*
 * sent status message for deck d to /xwax/status
 */

int osc_send_status(lo_address a, int d)
{
    struct deck *de;
    struct player *pl;
    struct track *tr;
    struct timecoder *tc;
    de = &osc_deck[d];
    pl = &de->player;
    tr = pl->track;
    tc = pl->timecoder;

    char *path;
    if(tr->path)
        path = tr->path;
    else
        path = "";

    printf("PORT: %s\n", lo_address_get_port(a));

    if(tr) {
        /* send a message to /xwax/status */
        if (lo_send(a, "/xwax/status", "isssfffi",
                de->ncontrol,           // deck number (int)
                path,               // track path (string)
                de->record->artist,     // artist name (string)
                de->record->title,      // track title (string)
                (float) tr->length / (float) tr->rate,  // track length in seconds (float)
                player_get_elapsed(pl),           // player position in seconds (float)
                pl->pitch,              // player pitch (float)
                pl->timecode_control)    // timecode activated or not (int)
            == -1) {
            printf("OSC error %d: %s\n", lo_address_errno(a),
                   lo_address_errstr(a));
        }
        printf("osc_send_status: sent deck %i status to %s\n", d, lo_address_get_url(a));
    }

    return 0;
}

/*
 * sent monitor message for deck d to /xwax/monitor
 */

int osc_send_monitor(lo_address a, int d)
{
    struct deck *de;
    struct player *pl;
    struct track *tr;
    struct timecoder *tc;
    de = &osc_deck[d];
    pl = &de->player;
    tr = pl->track;
    tc = pl->timecoder;

    char * mon;
    int r, c, i=0;
    mon = malloc((tc->mon_size+1)*tc->mon_size*sizeof(char));
    for (r=0; r<tc->mon_size; r++) {
        for (c=0; c<tc->mon_size; c++) {
            if ((int)tc->mon[r * tc->mon_size + c] > 0)
                mon[i++] = '1';
            else
                mon[i++] = '0';
        }
        mon[i++] = '\n';
    }
    mon[i++] = '\0';

    printf("PORT: %s\n", lo_address_get_port(a));

    if(tr) {
        /* send a message to /xwax/monitor */
        if (lo_send(a, "/xwax/monitor", "is", d, mon) == -1) {
            printf("OSC error %d: %s\n", lo_address_errno(a),
                   lo_address_errstr(a));
        }
        printf("osc_send_monitor: sent deck %i monitor to %s\n", d, lo_address_get_url(a));
    }

    free(mon);

    return 0;
}



/*
 * Disconnect time code control
 */

int disconnect_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    /* example showing pulling the argument values out of the argv array */
    printf("%s <- deck:%i\n", path, argv[0]->i);
    fflush(stdout);

    struct deck *de;
    struct player *pl;
    int d = argv[0]->i;

    if (d >= osc_ndeck) {
        error(255, path, "Trying to access into invalid deck");
        return 255;
    }

    de = &osc_deck[d];
    pl = &de->player;
    pl->timecode_control = false;
    return 0;
}

/*
 * reconnect time code control
 */

int reconnect_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    printf("%s <- deck:%i\n", path, argv[0]->i);
    fflush(stdout);

    struct deck *de;
    struct player *pl;
    int d = argv[0]->i;

    if (d >= osc_ndeck) {
        error(255, path, "Trying to access invalid deck");
        return 255;
    }

    de = &osc_deck[d];
    pl = &de->player;
    pl->timecode_control = true;
    return 0;
}

/*
 * recue the given deck
 */

int recue_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    printf("%s <- deck:%i\n", path, argv[0]->i);
    fflush(stdout);

    struct deck *de;
    int d = argv[0]->i;

    if (d >= osc_ndeck) {
        error(255, path, "Trying to access invalid deck");
        return 255;
    }

    de = &osc_deck[d];

    deck_recue(de);

    return 0;
}

/*
 * quit handler
 */
int quit_handler(const char *path, const char *types, lo_arg ** argv, int argc, void *data, void *user_data)
{
    lo_server_thread_free(st);
    raise(SIGINT);

    return 0;
}
