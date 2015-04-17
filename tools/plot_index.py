from print_btree import *
import math

def init_array(total):
    y_length= int(math.sqrt(total))
    x_length = total%y_length and total/y_length+1 or total/y_length
    arr=[]
    for i in xrange(y_length):
        y_arr=[]
        for j in xrange(x_length):
            y_arr.append(0)
        arr.append(y_arr)
    return arr, x_length, y_length

def calc_pos(page_no, x_length):
    return page_no%x_length , page_no/x_length

def gen_plot(total_arr, index_plot):
#    index_num = len(index_plot)+1
    import pylab
#    pylab.subplot(index_num,1,1)
    pylab.matshow(pylab.array(total_arr),cmap="gist_ncar_r")
    pylab.title('index: '+'all')
    i=2
    for index_id in index_plot.keys():
#        pylab.subplot(index_num,1,i)
        pylab.matshow(pylab.array(index_plot[index_id]),cmap="Reds")
        pylab.title('index: '+index_id)
        i+=1
    pylab.show()
#    pylab.savefig("d:/aaaa.png")


def plot_index(file_name):
    check_file = open(file_name, "rb")
    total = count_pages(file_name)
    start = time.time()
    total_arr, x_length, y_length = init_array(total)
    index_plot={}
    size=0
    index_color={}
    while True:
        page_content=check_file.read(UNIV_PAGE_SIZE)
        bytes = len(page_content)
        if bytes==0:
            break
        elif bytes!=UNIV_PAGE_SIZE:
            sys.stderr.write("bytes read (%d) doesn't match universal page size (%d)\n"%(bytes, UNIV_PAGE_SIZE))
            break
        
        page_no = get_fil_page_no(page_content)
        page_index_id_high , page_index_id_low = get_page_index_id(page_content)
        page_index_id = "h->%s|l->%s"%(page_index_id_high , page_index_id_low)
        page_record_num =get_page_record_number(page_content)

        if page_index_id_high!=0 or page_index_id_low!=0:
            x,y=calc_pos(page_no,x_length)
            #total
            if not index_color.has_key(page_index_id):
                index_color[page_index_id]=len(index_color)+1
            total_arr[y][x]=index_color[page_index_id]
            #index
            if not index_plot.has_key(page_index_id):
                index_plot[page_index_id]=init_array(total)[0]
            index_plot[page_index_id][y][x]=page_record_num

        if IS_VERBOSE and size%64==0 and time.time()-start>1:
            print "page %u okay: %.3f%% done"%(size, (float(size)/total*100))
            start = time.time()
        size+=1
    check_file.close()
    gen_plot(total_arr, index_plot)

if __name__ == '__main__':
    import optparse
    #    print_btree("d:/tmp/mysql/data/test/test.ibd")
    parser = optparse.OptionParser("plot_index.py [options] filename" )
    parser.add_option("-v", dest="verbose", action="store_true", default=False,
                      help="print verbose")

    (options, args) = parser.parse_args()
    if len(args)<=0:
        parser.print_help()
    else:
        if options.verbose:
            IS_VERBOSE=True
        plot_index(args[0])



