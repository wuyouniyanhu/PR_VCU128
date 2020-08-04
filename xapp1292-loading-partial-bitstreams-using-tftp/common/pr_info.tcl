source ../common/set_part.tcl

set static_dcp "Config_shift_right_count_up"

set partials  [list \
                    [dict create VSM vs_shift RM rm_shift_left  FILE Config_shift_left_count_down_pblock_shift_partial]\
                    [dict create VSM vs_shift RM rm_shift_right FILE Config_shift_right_count_up_pblock_shift_partial]\
                    [dict create VSM vs_count RM rm_count_up    FILE Config_shift_right_count_up_pblock_count_partial]\
                    [dict create VSM vs_count RM rm_count_down  FILE Config_shift_left_count_down_pblock_count_partial]\
                   ]
