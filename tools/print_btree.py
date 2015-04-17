#!/usr/bin/python
# wentong@taobao.com
# 2010-12-01
#

import sys
import time
import struct

UNIV_PAGE_SIZE   =       (2 * 8192)

PAGE_NO_LINK=0xFFFFFFFF

FIL_PAGE_SPACE_OR_CHKSUM =0
FIL_PAGE_OFFSET        =4
FIL_PAGE_PREV        =8
FIL_PAGE_NEXT        =12
FIL_PAGE_LSN    =    16
FIL_PAGE_TYPE    =    24
FIL_PAGE_FILE_FLUSH_LSN    = 26
FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID  = 34
FIL_PAGE_DATA        =38

PAGE_N_DIR_SLOTS = FIL_PAGE_DATA+0
PAGE_HEAP_TOP    = FIL_PAGE_DATA+2
PAGE_N_HEAP    = FIL_PAGE_DATA+4
PAGE_FREE=     FIL_PAGE_DATA+6
PAGE_N_RECS    = FIL_PAGE_DATA+16
PAGE_LEVEL = FIL_PAGE_DATA+26
PAGE_INDEX_ID = FIL_PAGE_DATA+28
PAGE_BTR_SEG_LEAF= FIL_PAGE_DATA+36
PAGE_BTR_SEG_TOP=FIL_PAGE_DATA+46

PAGE_DATA =FIL_PAGE_DATA+ 56

PAGE_NEW_INFIMUM =  PAGE_DATA+5
PAGE_NEW_SUPREMUM = PAGE_DATA+18

#--util

def mach_read_from_8(b):
    high = mach_read_from_4(b[0:4])
    low=   mach_read_from_4(b[4:8])
    return high, low

def mach_read_from_4(b):
    return(((b[0]) << 24)
    + ((b[1]) << 16)
    + ((b[2]) << 8)
    + (b[3]))

def mach_read_from_3(b):
    return(((b[0]) << 16)
    + ((b[1]) << 8)
    + (b[2]))

def mach_read_from_2(b):
    return(((b[0]) << 8)
    + (b[1]))

def mach_read_from_1(b):
    return (b[0])

def convert_ctype(str):
    return struct.unpack('%sB'%len(str), str)

def back_ctype(str):
    return struct.pack('%sB'%len(str), *str)

def get_offset_array(str, start, length):
    return convert_ctype(str[start:start+length])

#----fil

def get_fil_page_space_or_chksum(str):
    return mach_read_from_4(get_offset_array(str, FIL_PAGE_SPACE_OR_CHKSUM, FIL_PAGE_OFFSET-FIL_PAGE_SPACE_OR_CHKSUM))

def get_fil_page_no(str):
    return mach_read_from_4(get_offset_array(str, FIL_PAGE_OFFSET, FIL_PAGE_PREV-FIL_PAGE_OFFSET))

def get_fil_page_prev(str):
    return mach_read_from_4(get_offset_array(str, FIL_PAGE_PREV, 4))

def get_fil_page_next(str):
    return mach_read_from_4(get_offset_array(str, FIL_PAGE_NEXT, 4))

def get_fil_page_type(str):
    return mach_read_from_2(get_offset_array(str, FIL_PAGE_TYPE, FIL_PAGE_FILE_FLUSH_LSN-FIL_PAGE_TYPE))

#-----page
def get_dir_slots(str):
    return mach_read_from_2(get_offset_array(str, PAGE_N_DIR_SLOTS, 2))

def get_page_level(str):
    return mach_read_from_2(get_offset_array(str, PAGE_LEVEL, 2))

def get_page_index_id(str):
    return mach_read_from_8(get_offset_array(str, PAGE_INDEX_ID, 8))

def get_page_btr_top(str):
    return mach_read_from_4(get_offset_array(str, PAGE_BTR_SEG_TOP+0, 4)), mach_read_from_4(
            get_offset_array(str, PAGE_BTR_SEG_TOP+4, 4)), mach_read_from_2(
            get_offset_array(str, PAGE_BTR_SEG_TOP+8, 2))

def get_page_btr_leaf(str):
    return mach_read_from_4(get_offset_array(str, PAGE_BTR_SEG_LEAF+0, 4)), mach_read_from_4(
            get_offset_array(str, PAGE_BTR_SEG_LEAF+4, 4)), mach_read_from_2(
            get_offset_array(str, PAGE_BTR_SEG_LEAF+8, 2))

def get_page_record_number(str):
    return  mach_read_from_2(get_offset_array(str, PAGE_N_RECS, 2))

def get_page_heap_info(str):
    return mach_read_from_2(get_offset_array(str, PAGE_HEAP_TOP, 2)), mach_read_from_2(
            get_offset_array(str, PAGE_N_HEAP, 2))

def get_page_free(str):
    return mach_read_from_2(get_offset_array(str, PAGE_FREE, 2))

def get_infimum(str):
    return back_ctype(get_offset_array(str, PAGE_NEW_INFIMUM, 7))

def get_supremum(str):
    return back_ctype(get_offset_array(str, PAGE_NEW_SUPREMUM, 8))


#------uitl

def find_first_one(dict):
    for key in dict.keys():
        if dict[key]["prev"] == PAGE_NO_LINK:
            return key
    return None


IS_DEGUB = False
MAX_PRINT_LEVEL = None
IS_WHOLE = False
IS_VERBOSE =  False

MAX_RECORD_IN_PAGE = UNIV_PAGE_SIZE/5

def print_level_btree(index_tree):
    index_score={}
    print "Btree:"
    index_id_keys = index_tree.keys()
    index_id_keys.reverse()
    for b_index_id in index_id_keys:
        index_score[b_index_id]=0
        max_level=None
        print "index id:", b_index_id
        b_tree=[]
        b_index_keys = index_tree[b_index_id].keys()
        b_index_keys.reverse()
        for b_index_level in b_index_keys:
            if max_level is None:
                max_level=b_index_level
            if MAX_PRINT_LEVEL and (max_level-b_index_level)>=MAX_PRINT_LEVEL:
                continue
            if IS_WHOLE:
                b_tree.append([])
            else:
                print "level:",b_index_level
            max_level_page_record_num=0
            all_level_score=[]
            search_page_no=find_first_one(index_tree[b_index_id][b_index_level])
            is_end=False
            while (not is_end) and search_page_no is not None:
                page = index_tree[b_index_id][b_index_level][search_page_no]

                if IS_WHOLE:
                    b_tree[len(b_tree)-1].append(page)
                else:
                    print "  "*(max_level-b_index_level), "|no:%s(%s)"%(page["no"], page["num"])

                if page["next"] == PAGE_NO_LINK:
                    is_end=True
                else:
                    search_page_no=page["next"]

                all_level_score.append(MAX_RECORD_IN_PAGE-page["num"])
                if  page["num"]> max_level_page_record_num:
                    max_level_page_record_num = page["num"]
            if len(all_level_score)>0:
                score_fix = MAX_RECORD_IN_PAGE -max_level_page_record_num
                index_score[b_index_id]+=reduce(lambda a, b:a+b, map(lambda arg:arg-score_fix, all_level_score), 0)/len(
                        all_level_score)*(b_index_level+1)



            #print tree
        if len(b_tree)>0 and len(b_tree[0])>0:
            print_level_offset=[0]
            print_level_length=[len(b_tree[0])]
            def print_tree_r(print_level_now):
                if len(b_tree[print_level_now])>0 and print_level_offset[print_level_now] < len(b_tree[print_level_now]
                ):
                    for i in xrange(print_level_length[print_level_now]):
                    #                        print print_level_now,print_level_offset[print_level_now],i
                        page = b_tree[print_level_now][print_level_offset[print_level_now]+i]
                        print "|  "*(print_level_now)+"|--no:%s(%s)"%(page["no"], page["num"])
                        if len(b_tree)>print_level_now+1 and len(b_tree[print_level_now+1])>0:
                            if len(print_level_offset)<= print_level_now+1:
                                print_level_offset.append(0)
                                print_level_length.append(page["num"])
                            else:
                                print_level_length[print_level_now+1]=page["num"]
                            print_tree_r(print_level_now+1)
                            print_level_offset[print_level_now+1]+=page["num"]
                        pass

            print_tree_r(0)
        print "---------------------------"
    print "index_score:"
    for index_id in index_score.keys():
        print "   index id:%s 's score:%s"%(index_id,index_score[index_id])

def count_pages(file_path):
    import os
    st=os.stat(file_path)
    return  st.st_size/UNIV_PAGE_SIZE

def print_btree(file_name):
    check_file = open(file_name, "rb")
    size=0
    all_index_id={}
    index_tree={}
    total = count_pages(file_name)
    start = time.time()
    while True:
        page_content=check_file.read(UNIV_PAGE_SIZE)
        bytes = len(page_content)
        if bytes==0:
            break
        elif bytes!=UNIV_PAGE_SIZE:
            sys.stderr.write("bytes read (%d) doesn't match universal page size (%d)\n"%(bytes, UNIV_PAGE_SIZE))
            break
        if IS_DEGUB:
            print "page no:%d"%size
        page_type= get_fil_page_type(page_content)
        space_id = get_fil_page_space_or_chksum(page_content)
        page_no = get_fil_page_no(page_content)
        if IS_DEGUB:
            print "page addr:%s , page type:%s, page space id:%s ,page no:%s"%((hex(size*UNIV_PAGE_SIZE
            )+"h"), page_type, space_id, page_no)

        page_level = get_page_level(page_content)
        page_index_id_high , page_index_id_low = get_page_index_id(page_content)
        page_index_id = "h->%s|l->%s"%(page_index_id_high , page_index_id_low)
        page_btr_top = get_page_btr_top(page_content)
        page_btr_leaf=get_page_btr_leaf(page_content)
        if IS_DEGUB:
            print "page level:%s ,page index id:%s ,page_btr_top:%s ,page_btr_leaf:%s"%(page_level, page_index_id, page_btr_top, page_btr_leaf)

        page_file_prev = get_fil_page_prev(page_content)
        page_file_next = get_fil_page_next(page_content)
        if IS_DEGUB:
            print "page_file_prev:%s ,page_file_next:%s"%(page_file_prev, page_file_next)

        page_record_num =get_page_record_number(page_content)
        dir_slots = get_dir_slots(page_content)
        if IS_DEGUB:
            print "page record number:%s ,dir_slots:%s"% (page_record_num, dir_slots)

        page_heap_top, page_heap_num = get_page_heap_info(page_content)
        page_free = get_page_free(page_content)
        if IS_DEGUB:
            print "page free:%s ,page_heap_top:%s ,page_heap_num:%s "% (page_free, page_heap_top, page_heap_num)
        if(IS_DEGUB and page_type==17855):
            page_infimum=get_infimum(page_content)
            page_supremum=get_supremum(page_content)
            print "page infimum:%s ,page supremum:%s"%(page_infimum, page_supremum)

        if not all_index_id.has_key(page_index_id):
            all_index_id[page_index_id]=0
        all_index_id[page_index_id]+=1

        if page_index_id_high!=0 or page_index_id_low!=0:
            if not index_tree.has_key(page_index_id):
                index_tree[page_index_id]={}
            if not index_tree[page_index_id].has_key(page_level):
                index_tree[page_index_id][page_level]={}
            index_tree[page_index_id][page_level][page_no]={"no":page_no, "num":page_record_num, "prev":page_file_prev ,
                                                            "next":page_file_next}
        size+=1

        if IS_VERBOSE and size%64==0 and time.time()-start>1:
            print "page %u okay: %.3f%% done"%(size, (float(size)/total*100))
            start = time.time()
        if IS_DEGUB:
            print "----------------------------"
    check_file.close()
    print "all index id's page num:"
    for index_id in all_index_id.keys():
        print "   id:%s has page num:%s"%(index_id,all_index_id[index_id])

    print_level_btree(index_tree)


if __name__ == '__main__':
    import optparse
    #    print_btree("d:/tmp/mysql/data/test/test.ibd")
    parser = optparse.OptionParser("print_btree.py [options] filename" )
    parser.add_option("-m", dest="max", action="store", default=None, type="long",
                      help="set the max level to print")
    parser.add_option("-d", dest="debug", action="store_true", default=False,
                      help="debug mode (prints info for each page)")
    parser.add_option("-w", dest="whole", action="store_true", default=False,
                      help="print whole b-tree")
    parser.add_option("-v", dest="verbose", action="store_true", default=False,
                      help="print verbose")

    (options, args) = parser.parse_args()
    if len(args)<=0:
        parser.print_help()
    else:
        if options.max:
            MAX_PRINT_LEVEL=options.max
        if options.debug:
            IS_DEGUB=True
        if options.whole:
            IS_WHOLE=True
        if options.verbose:
            IS_VERBOSE=True
        print_btree(args[0])


