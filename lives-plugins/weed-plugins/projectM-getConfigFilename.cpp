//
// File:   getConfigFilename.cpp
//
// Author: fatray
//
// Created on 05 December 2007, 23:39
//
// FIXME: portability


// I hacked include<string> on to silence my compiler, is it valid?
#include <string>
#include <cstring>
#include <cstdlib>
#include "projectM-getConfigFilename.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <cstdio>

// get the full pathname of a configfile
std::string getConfigFilename() {
  char num[512];
  FILE *in;
  FILE *out;

  char *home;
  // FIXME: fixed length buffers are not ideal.
  char projectM_home[1024];
  char projectM_config[1024];

  strcpy(projectM_config, PROJECTM_PREFIX);
  strcpy(projectM_config + strlen(PROJECTM_PREFIX), CONFIG_FILE);
  projectM_config[strlen(PROJECTM_PREFIX) + strlen(CONFIG_FILE)] = '\0';
  fprintf(stderr, "dir:%s \n", projectM_config);
  home = getenv("HOME");
  strcpy(projectM_home, home);
  strcpy(projectM_home + strlen(home), "/.projectM/config.inp");
  projectM_home[strlen(home) + strlen("/.projectM/config.inp")] = '\0';

  if ((in = fopen(projectM_home, "r"))) {
    fprintf(stderr, "reading ~/.projectM/config.inp \n");
    fclose(in);
    return std::string(projectM_home);
  }

  fprintf(stderr, "trying to create ~/.projectM/config.inp \n");

  projectM_home[strlen(home) + strlen("/.projectM")] = '\0';
  mkdir(projectM_home, 0755);

  strcpy(projectM_home + strlen(home), "/.projectM/config.inp");
  projectM_home[strlen(home) + strlen("/.projectM/config.inp")] = '\0';

  if ((out = fopen(projectM_home, "w"))) {
    if ((in = fopen(projectM_config, "r"))) {
      while (fgets(num, 80, in) != NULL) {
        fputs(num, out);
      }
      fclose(in);
      fclose(out);


      if ((in = fopen(projectM_home, "r"))) {
        fprintf(stderr, "created ~/.projectM/config.inp successfully\n");
        fclose(in);
        return std::string(projectM_home);
      }

      fprintf(stderr, "This shouldn't happen, using implementation defaults\n");
      abort();
    }
    fprintf(stderr, "Cannot find projectM default config, using implementation defaults\n");
    abort();
  }

  fprintf(stderr, "Cannot create ~/.projectM/config.inp, using default config file\n");
  if ((in = fopen(projectM_config, "r"))) {
    fprintf(stderr, "Successfully opened default config file\n");
    fclose(in);
    return std::string(projectM_config);
  }

  fprintf(stderr, "Using implementation defaults, your system is really messed up, I'm surprised we even got this far\n");
  return NULL;
}

