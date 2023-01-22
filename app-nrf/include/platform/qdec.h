#pragma once

typedef void (*qdec_data_cb)(int value);

void qdec_set_data_cb(qdec_data_cb cb);
