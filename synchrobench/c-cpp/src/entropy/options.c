#include "options.h"
#include <string.h>

options_t read_options(int argc, char** argv) {
  struct option long_options[] = {
    // These options don't set a flag
    {"help",                      no_argument,       NULL, 'h'},
    {"thread-num",                required_argument, NULL, 't'},
    {"range",                     required_argument, NULL, 'R'},
    {"seed",                      required_argument, NULL, 'S'},
    {"add-operations",            required_argument, NULL, 'a'},
    {"del-operations",            required_argument, NULL, 'd'},
    {"read-operations",           required_argument, NULL, 'r'},
    {"output-prefix",             required_argument, NULL, 'o'},
    {NULL, 0, NULL, 0}
  };

  options_t options;
  options.thread_num = DEFAULT_THREAD_NUM;
  options.range = DEFAULT_ELEMENT_RANGE;
  options.seed = DEFAULT_SEED;
  options.add_operations = DEFAULT_ADD_OPERATIONS;
  options.del_operations = DEFAULT_DEL_OPERATIONS;
  strcpy(options.filename, DEFAULT_OUTPUT);

  int i, c;
  while(1) {
    i = 0;
    c = getopt_long(argc, argv, "ht:R:S:a:d:r:o:", long_options, &i);
		
    if(c == -1)
      break;
		
    if(c == 0 && long_options[i].flag == 0)
      c = long_options[i].val;
		
    switch(c) {
    case 0:
      /* Flag is automatically set */
      break;
    case 'h':
      printf("intset - quantifiable entropy test "
	     "(linked list)\n"
	     "\n"
	     "Usage:\n"
	     "  entropy [options...]\n"
	     "\n"
	     "Options:\n"
	     "  -h, --help\n"
	     "        Print this message\n"
	     "  -t, --thread-num <int>\n"
	     "        Number of threads (default=" XSTR(DEFAULT_THREAD_NUM) ")\n"
	     "  -R, --range <int>\n"
	     "        Range of integer values inserted in set (default=" XSTR(DEFAULT_RANGE) ")\n"
	     "  -S, --seed <int>\n"
	     "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
	     "  -a, --add-operations <int>\n"
	     "        Total add operations (default=" XSTR(DEFAULT_ADD_OPERATIONS) ")\n"
	     "  -d, --del-operations <int>\n"
	     "        Total delete operations (default=" XSTR(DEFAULT_DEL_OPERATIONS) ")\n"
	     "  -r, --read-operations <int>\n"
	     "        Total read operations (default=" XSTR(DEFAULT_READ_OPERATIONS) ")\n"
	     "  -o, --output-prefix <int>\n"
	     "        output file (default=" DEFAULT_OUTPUT ")\n"
	     );
      exit(0);
    case 'd':
      options.del_operations = atoi(optarg);
      break;
    case 't':
      options.thread_num = atoi(optarg);
      break;
    case 'R':
      options.range = atol(optarg);
      break;
    case 'S':
      options.seed = atoi(optarg);
      break;
    case 'a':
      options.add_operations = atoi(optarg);
      break;
    case 'r':
      options.read_operations = atoi(optarg);
      break;
    case 'o':
      strcpy(options.filename, optarg);
    default:
      exit(1);
    }
  }

  assert(options.thread_num >= 0);
  assert(options.range > 0);
  assert(options.add_operations >= 0);
  assert(options.del_operations >= 0);
  assert(options.read_operations >= 0);
  assert(options.add_operations || options.del_operations || options.read_operations);
  return options;
}
