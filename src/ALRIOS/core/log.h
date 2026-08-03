/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef AR_LOG_H
#define AR_LOG_H

#define RST "\033[0m"
#define RED "\033[31m"
#define GRN "\033[32m"
#define YLW "\033[33m"
#define BLU "\033[34m"
#define MAG "\033[35m"
#define CYN "\033[36m"
#define BLD "\033[1m"
#define DIM "\033[2m"

void alri_printf(const char *format, ...);
void alri_print(const char *format, ...);
void alri_print_force(const char *format, ...);

#endif
