/*
 * Copyright (c) 2015 Mindaugas Rasiukevicius <rmind at netbsd org>
 * All rights reserved.
 *
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <err.h>

#include "masstree.c"

static masstree_t *		tree;
static pthread_barrier_t	barrier;
static unsigned			nworkers;

static void *
fuzz_put_del(void *arg)
{
	const unsigned id = (uintptr_t)arg;
	// unsigned n = 1000 * 1000;
	unsigned n = 1000;

	pthread_barrier_wait(&barrier);
	while (n--) {
		/*
		 * Key range of 32 values to trigger many contended
		 * splits and collapse within the same layer.
		 */
		uint64_t key = random() & 0x1f;

		if (random() & 0x1) {
			masstree_put(tree, &key, sizeof(key), (void *)1);
		} else {
			masstree_del(tree, &key, sizeof(key));
		}
		// printf("fuzz_put_del: worker %u, key %lu, n : %lu\n", id, key, n);
	}
	pthread_barrier_wait(&barrier);

	/* The primary thread performs the clean-up. */
	if (id == 0) for (uint64_t key = 0; key <= 0x1f; key++) {
		masstree_del(tree, &key, sizeof(key));
	}
	pthread_exit(NULL);
	return NULL;
}

static void *
fuzz_multi(void *arg)
{
	const unsigned id = (uintptr_t)arg;
	// unsigned n = 1000 * 1000;
	unsigned n = 1000;


	pthread_barrier_wait(&barrier);
	while (n--) {
		/*
		 * Key range of 4k will create multiple internode
		 * layers with some contention amongst them.
		 */
		uint64_t key = random() & 0xfff;
		void *val;

		switch (random() % 3) {
		case 0:
			val = masstree_get(tree, &key, sizeof(key));
			assert(!val || (uintptr_t)val == (uintptr_t)key);
			break;
		case 1:
			masstree_put(tree, &key, sizeof(key),
			    (void *)(uintptr_t)key);
			break;
		case 2:
			masstree_del(tree, &key, sizeof(key));
			break;
		}
		// printf("fuzz_multi: worker %u, key %lu, n : %lu\n", id, key, n);
	}
	pthread_barrier_wait(&barrier);

	if (id == 0) for (uint64_t key = 0; key <= 0xfff; key++) {
		masstree_del(tree, &key, sizeof(key));
	}
	pthread_exit(NULL);
	return NULL;
}

static void *
fuzz_layers(void *arg)
{
	const unsigned id = (uintptr_t)arg;
	// unsigned n = 1000 * 1000;
	unsigned n = 100;

	pthread_barrier_wait(&barrier);
	while (n--) {
		uint64_t key[2];
		uintptr_t numval;
		void *val;

		// printf("fuzz_layers: worker %u, n : %lu\n", id, n);
		/*
		 * Two layers, contended to cause concurrent splits
		 * and collapses.
		 */
		key[0] = random() & 0xf;
		key[1] = random() & 0x1f;
		numval = key[0] ^ key[1];

		switch (random() % 3) {
		case 0:
			val = masstree_get(tree, key, sizeof(key));
			assert(!val || (uintptr_t)val == numval);
			break;
		case 1:
			masstree_put(tree, key, sizeof(key), (void *)numval);
			break;
		case 2:
			masstree_del(tree, key, sizeof(key));
			break;
		}
	}
	pthread_barrier_wait(&barrier);

	if (id == 0) {
		for (uint64_t k1 = 0; k1 <= 0xf; k1++) {
			for (uint64_t k2 = 0; k2 <= 0x1f; k2++) {
				uint64_t key[2] = { k1, k2 };
				masstree_del(tree, key, sizeof(key));
				// printf("cleaning up key %lu %lu\n", k1, k2);
			}
		}
	}
	pthread_exit(NULL);
	return NULL;
}

static void
run_test(void *func(void *))
{
	pthread_t *thr;
	void *ref;

	srandom(1);
	tree = masstree_create(NULL);
	nworkers = sysconf(_SC_NPROCESSORS_CONF) + 1;

	thr = malloc(sizeof(pthread_t) * nworkers);
	pthread_barrier_init(&barrier, NULL, nworkers);

	for (unsigned i = 0; i < nworkers; i++) {
		if ((errno = pthread_create(&thr[i], NULL,
		    func, (void *)(uintptr_t)i)) != 0) {
			err(EXIT_FAILURE, "pthread_create");
		}
	}
	for (unsigned i = 0; i < nworkers; i++) {
		pthread_join(thr[i], NULL);
	}
	pthread_barrier_destroy(&barrier);

	ref = masstree_gc_prepare(tree);
	masstree_gc(tree, ref);
	masstree_destroy(tree);
}

int
main(void)
{
	run_test(fuzz_put_del);
	puts("fuzz_put_del ok");
	run_test(fuzz_multi);
	puts("fuzz_multi ok");
	run_test(fuzz_layers);
	puts("fuzz_layers ok");
	puts("all ok");
	return 0;
}
