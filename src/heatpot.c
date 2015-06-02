/*
	heatpot.c

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sim_avr.h"
#include "sim_time.h"

#include "heatpot.h"

static avr_cycle_count_t
heatpot_evaluate_timer(
		struct avr_t * avr,
        avr_cycle_count_t when,
        void * param)
{
	heatpot_p  p = (heatpot_p) param;

	float weight = 0.0;

	for (int si = 0; si < ARRAY_SIZE(p->tally); si++)
		weight += p->tally[si].cost;

	float delta = p->current - p->ambiant;
	float noise = (float)((random() % 32) - 16) / 32.0;
	delta += noise;

	float cost = ((delta/2) + noise) * -weight;

	if (delta < 0) {
		cost = (-delta / 10);
	} else {
		cost = weight > 0 ? 0.1 : -0.1;
	}
	p->current += cost;
//	printf("%s w=%.3f d=%.3f c=%.3f = %.3f\n", p->name,
//		weight, delta, cost, p->current);

	avr_raise_irq(p->irq + IRQ_HEATPOT_TEMP_OUT, p->current * 256);

	return when + p->cycle;
}

static void
heatpot_tally_in_hook(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
	heatpot_p p = (heatpot_p)param;
	heatpot_data_t v = {.v = value };

	heatpot_tally(p, v.sid, v.cost);
}

static const char * irq_names[IRQ_HEATPOT_COUNT] = {
	[IRQ_HEATPOT_TALLY] = "8<heatpot.tally",
};

void
heatpot_init(
		struct avr_t * avr,
		heatpot_p p,
		const char * name,
		float ambiant )
{
	memset(p, 0, sizeof(*p));
	p->avr = avr;
	strcpy(p->name, (char*)name);
	p->irq = avr_alloc_irq(&avr->irq_pool, 0, IRQ_HEATPOT_COUNT, irq_names);
	avr_irq_register_notify(p->irq + IRQ_HEATPOT_TALLY, heatpot_tally_in_hook, p);

	p->cycle = avr_usec_to_cycles(avr, HEATPOT_RESAMPLE_US);
	avr_cycle_timer_register_usec(avr, p->cycle, heatpot_evaluate_timer, p);

	p->ambiant = p->current = ambiant;
}

void
heatpot_tally(
		heatpot_p p,
		uint8_t sid,
		float cost )
{
	p->tally[sid].cost = cost;
}
