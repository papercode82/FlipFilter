/* -*- P4_16 -*- */
#include<core.p4>
#if __TARGET_TOFINO__ == 2
#include<t2na.p4>
#else
#include<tna.p4>
#endif

#define UNIT_SIZE 8
#define FILTER_UNITS1 1<<13
#define FILTER_UNITS2 1<<13
#define CNT_NUM1 2
#define CNT_NUM2 4
#define BIT_NUM 1<<13

const bit<16> TYPE_IPV4 = 0x800;

/*************************************************************************
*********************** H E A D E R S  ***********************************
*************************************************************************/

typedef bit<9>  egressSpec_t;
typedef bit<48> macAddr_t;
typedef bit<32> ip4Addr_t;

header ethernet_t {
    macAddr_t dstAddr;
    macAddr_t srcAddr;
    bit<16>   etherType;
}

header ipv4_t {
    bit<4>    version;
    bit<4>    ihl;
    bit<8>    diffserv;
    bit<16>   totalLen;
    bit<16>   identification;
    bit<3>    flags;
    bit<13>   fragOffset;
    bit<8>    ttl;
    bit<8>    protocol;
    bit<16>   hdrChecksum;
    ip4Addr_t srcAddr;
    ip4Addr_t dstAddr;
}

header udp_h {
	bit<16> src_port;
	bit<16> dst_port;
	bit<16> total_len;
	bit<16> checksum;
}

struct egress_headers_t {}
struct egress_metadata_t {}

header my_flow_h {
	bit<32> id;
#ifdef __TEST__
	bit<32> timestamp;
#endif
}

header my_record_h {
    bit<8> func_index;
    bit<32> cnt_index;
    bit<8>  opt;
    bit<8>  bit_idx;
    bit<8> func_index2;
    bit<32> cnt_index2;
    bit<8>  opt2;
}

struct metadata{
    bit<8> tag_l1_c1;
    bit<8> tag_l1_c2;
    bit<8> overflow_tag_L2;
    bit<8> tag_l2_c1;
    bit<8> tag_l2_c2;
    bit<8> tag_l2_c3;
    bit<8> tag_l2_c4;
    bit<8> overflow_tag_L3;
}

struct headers {
    ethernet_t   ethernet;
    ipv4_t       ipv4;
    udp_h        udp;
    my_flow_h   myflow;
	my_record_h myrecord;
}

/*************************************************************************
*********************** P A R S E R  ***********************************
*************************************************************************/

parser MyParser(packet_in packet,
                out headers hdr,
                out metadata meta,
                out ingress_intrinsic_metadata_t ig_intr_md) {

    state start {
		packet.extract(ig_intr_md);
		packet.advance(PORT_METADATA_SIZE);
		transition parse_ethernet;
    }
	
	state parse_ethernet {
		packet.extract(hdr.ethernet);
        transition select(hdr.ethernet.etherType){
            TYPE_IPV4: parse_ipv4;
            default: accept;
        }
	}
	
    state parse_ipv4 {
        packet.extract(hdr.ipv4);
        transition select(hdr.ipv4.protocol)
        {
            (bit<8>) 17 : parse_udp;
            default : accept;
        }
    }

    state parse_udp {
		packet.extract(hdr.udp);
		transition parse_myflow;
	}

	state parse_myflow {
		packet.extract(hdr.myflow);
		transition select(ig_intr_md.resubmit_flag) {
			0: parse_origin;
			1: parse_resubmit;
		}
	}

	state parse_origin {
		hdr.myrecord.setValid();
		transition accept;
	}
	
	state parse_resubmit {
		packet.extract(hdr.myrecord);
		transition accept;
	}

}

/*************************************************************************
**************  I N G R E S S   P R O C E S S I N G   *******************
*************************************************************************/

control MyIngress(inout headers hdr,
                  inout metadata meta,
                  in ingress_intrinsic_metadata_t ig_intr_md,
				  in ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
				  inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
				  inout ingress_intrinsic_metadata_for_tm_t ig_tm_md
					) {

    // hash functions used in different algorithms
	CRCPolynomial<bit<32>>(coeff=0x04C11DB7,reversed=true, msb=false, extended=false, init=0xFFFFFFFF, xor=0xFFFFFFFF) crc_func;
	Hash<bit<32>>(HashAlgorithm_t.CUSTOM, crc_func) hash_func;

	CRCPolynomial<bit<32>>(coeff=0x1131AA27,reversed=true, msb=false, extended=false, init=0xFFFFFFFF, xor=0xFFFFFFFF) crc_pos;
	Hash<bit<32>>(HashAlgorithm_t.CUSTOM, crc_pos) hash_pos;

    CRCPolynomial<bit<32>>(coeff=0x76313325,reversed=true, msb=false, extended=false, init=0xFFFFFFFF, xor=0xFFFFFFFF) crc_opt;
	Hash<bit<8>>(HashAlgorithm_t.CUSTOM, crc_opt) hash_opt;

    CRCPolynomial<bit<32>>(coeff=0x56112233,reversed=true, msb=false, extended=false, init=0xFFFFFFFF, xor=0xFFFFFFFF) crc_cnt;
	Hash<bit<8>>(HashAlgorithm_t.CUSTOM, crc_cnt) hash_cnt;

    // get index of hash functions
    action ac_get_func_index() {
        // two hash functions
        hdr.myrecord.func_index = (bit<8>) hash_func.get({hdr.ipv4.srcAddr, hdr.ipv4.dstAddr})[0:0];
    }

    table tb_get_func_index {
        actions = {
            ac_get_func_index;
        }
        size = 1;
        const default_action = ac_get_func_index;
    }

    // get physical index of L1
    action ac_get_pos_L1() {
        hdr.myrecord.cnt_index = (bit<32>) hash_pos.get({hdr.ipv4.srcAddr, hdr.myrecord.func_index})[12:0];
    }

    table tb_get_pos_L1 {
        actions = {
            ac_get_pos_L1;
        }
        size = 1;
        const default_action = ac_get_pos_L1;
    }

    // get opt in hash counter
    action ac_get_opt_cnt() {
        hdr.myrecord.opt = (bit<8>) hash_opt.get({hdr.ipv4.srcAddr, hdr.myrecord.cnt_index})[0:0];
    }

    table tb_get_opt_cnt {
        actions = {
            ac_get_opt_cnt;
        }
        size = 1;
        const default_action = ac_get_opt_cnt;
    }

    // get index in each counter
    action ac_get_bit_index1() {
        hdr.myrecord.bit_idx = (bit<8>) hash_cnt.get({hdr.ipv4.srcAddr, hdr.ipv4.dstAddr})[1:0];
    }

    table tb_get_bit_index1 {
        actions = {
            ac_get_bit_index1;
        }
        size = 1;
        const default_action = ac_get_bit_index1;
    }

    Register<bit<UNIT_SIZE>, bit<32>>(FILTER_UNITS1) L1C1;

    RegisterAction<bit<UNIT_SIZE>, bit<32>, bit<UNIT_SIZE>>(L1C1) reg_update_L1_C1 = {
		void apply(inout bit<UNIT_SIZE> register_data, out bit<UNIT_SIZE> result) {
            if (hdr.myrecord.opt == register_data) {
                result = 0x1;
                register_data = register_data;
            } else {
                if (hdr.myrecord.bit_idx[0:0] == 0b1) {
                    result = 0x0;
                    register_data = register_data ^ 1;
                } else {
                    result = 0x0;
                    register_data = register_data;
                }
            }
		}
	};

    action ac_update_L1_C1() {
        meta.tag_l1_c1 = reg_update_L1_C1.execute(hdr.myrecord.cnt_index);
    }

    table tb_update_L1_C1 {
        actions = {
            ac_update_L1_C1;
        }
        size = 1;
        const default_action = ac_update_L1_C1;
    }

    Register<bit<UNIT_SIZE>, bit<32>>(FILTER_UNITS1) L1C2;

    RegisterAction<bit<UNIT_SIZE>, bit<32>, bit<UNIT_SIZE>>(L1C2) reg_update_L1_C2 = {
		void apply(inout bit<UNIT_SIZE> register_data, out bit<UNIT_SIZE> result) {
            if (hdr.myrecord.opt == register_data) {
                result = 0x1;
                register_data = register_data;
            } else {
                if (hdr.myrecord.bit_idx[0:0] == 0b0) {
                    result = 0x0;
                    register_data = register_data ^ 1;
                } else {
                    result = 0x0;
                    register_data = register_data;
                }
            }
		}
	};

    action ac_update_L1_C2() {
        meta.tag_l1_c2 = reg_update_L1_C2.execute(hdr.myrecord.cnt_index);
    }

    table tb_update_L1_C2 {
        actions = {
            ac_update_L1_C2;
        }
        size = 1;
        const default_action = ac_update_L1_C2;
    }

    action ac_set_overflow_tag() {
        meta.overflow_tag_L2 = 1;
    }

    table tb_go_to_L2 {
        key = {
            meta.tag_l1_c1 : exact;
            meta.tag_l1_c2 : exact;
        }
        actions = {
            ac_set_overflow_tag;
            NoAction;
        }
        size = 8;
        const default_action = NoAction;
        const entries = {
            (0,0) : NoAction;
            (0,1) : NoAction;
            (1,0) : NoAction;
            (1,1) : ac_set_overflow_tag;
        }
    }

    /*
        L2 filter
    */
    action ac_get_func_index2() {
        // two hash functions
        hdr.myrecord.func_index2 = (bit<8>) hash_func.get({hdr.ipv4.srcAddr, hdr.ipv4.dstAddr})[1:0];
    }

    table tb_get_func_index2 {
        actions = {
            ac_get_func_index2;
        }
        size = 1;
        const default_action = ac_get_func_index2;
    }

    // get physical index of L2
    CRCPolynomial<bit<32>>(coeff=0x4411CC42,reversed=true, msb=false, extended=false, init=0xFFFFFFFF, xor=0xFFFFFFFF) crc_pos2;
	Hash<bit<32>>(HashAlgorithm_t.CUSTOM, crc_pos2) hash_pos2;
    action ac_get_pos_L2() {
        hdr.myrecord.cnt_index2 = (bit<32>) hash_pos2.get({hdr.ipv4.srcAddr, hdr.myrecord.func_index2})[12:0];
    }

    table tb_get_pos_L2 {
        actions = {
            ac_get_pos_L2;
        }
        size = 1;
        const default_action = ac_get_pos_L2;
    }

    // get opt in hash counter
    CRCPolynomial<bit<32>>(coeff=0x31AA3B2C,reversed=true, msb=false, extended=false, init=0xFFFFFFFF, xor=0xFFFFFFFF) crc_opt2;
	Hash<bit<8>>(HashAlgorithm_t.CUSTOM, crc_opt2) hash_opt2;
    action ac_get_opt_cnt2() {
        hdr.myrecord.opt2 = (bit<8>) hash_opt2.get({hdr.ipv4.srcAddr, hdr.myrecord.cnt_index2})[0:0];
    }

    table tb_get_opt_cnt2 {
        actions = {
            ac_get_opt_cnt2;
        }
        size = 1;
        const default_action = ac_get_opt_cnt2;
    }

    Register<bit<UNIT_SIZE>, bit<32>>(FILTER_UNITS2) L2C1;

    RegisterAction<bit<UNIT_SIZE>, bit<32>, bit<UNIT_SIZE>>(L2C1) reg_update_L2_C1 = {
		void apply(inout bit<UNIT_SIZE> register_data, out bit<UNIT_SIZE> result) {
            if (hdr.myrecord.opt2 == register_data) {
                result = 0x1;
                register_data = register_data;
            } else {
                if (hdr.myrecord.bit_idx == 0) {
                    result = 0x0;
                    register_data = register_data ^ 1;
                } else {
                    result = 0x0;
                    register_data = register_data;
                }
            }
		}
	};

    action ac_update_L2_C1() {
        meta.tag_l2_c1 = reg_update_L2_C1.execute(hdr.myrecord.cnt_index2);
    }

    table tb_update_L2_C1 {
        key = {
            meta.overflow_tag_L2 : exact;
        }
        actions = {
            ac_update_L2_C1;
            NoAction;
        }
        size = 8;
        const default_action = NoAction;
        const entries = {
            1 : ac_update_L2_C1;
        }
    }

    Register<bit<UNIT_SIZE>, bit<32>>(FILTER_UNITS2) L2C2;

    RegisterAction<bit<UNIT_SIZE>, bit<32>, bit<UNIT_SIZE>>(L2C2) reg_update_L2_C2 = {
		void apply(inout bit<UNIT_SIZE> register_data, out bit<UNIT_SIZE> result) {
            if (hdr.myrecord.opt2 == register_data) {
                result = 0x1;
                register_data = register_data;
            } else {
                if (hdr.myrecord.bit_idx == 1) {
                    result = 0x0;
                    register_data = register_data ^ 1;
                } else {
                    result = 0x0;
                    register_data = register_data;
                }
            }
		}
	};

    action ac_update_L2_C2() {
        meta.tag_l2_c2 = reg_update_L2_C2.execute(hdr.myrecord.cnt_index2);
    }

    table tb_update_L2_C2 {
        key = {
            meta.overflow_tag_L2 : exact;
        }
        actions = {
            ac_update_L2_C2;
            NoAction;
        }
        size = 8;
        const default_action = NoAction;
        const entries = {
            1 : ac_update_L2_C2;
        }
    }

    Register<bit<UNIT_SIZE>, bit<32>>(FILTER_UNITS2) L2C3;

    RegisterAction<bit<UNIT_SIZE>, bit<32>, bit<UNIT_SIZE>>(L2C3) reg_update_L2_C3 = {
		void apply(inout bit<UNIT_SIZE> register_data, out bit<UNIT_SIZE> result) {
            if (hdr.myrecord.opt2 == register_data) {
                result = 0x1;
                register_data = register_data;
            } else {
                if (hdr.myrecord.bit_idx == 2) {
                    result = 0x0;
                    register_data = register_data ^ 1;
                } else {
                    result = 0x0;
                    register_data = register_data;
                }
            }
		}
	};

    action ac_update_L2_C3() {
        meta.tag_l2_c3 = reg_update_L2_C3.execute(hdr.myrecord.cnt_index2);
    }

    table tb_update_L2_C3 {
        key = {
            meta.overflow_tag_L2 : exact;
        }
        actions = {
            ac_update_L2_C3;
            NoAction;
        }
        size = 8;
        const default_action = NoAction;
        const entries = {
            1 : ac_update_L2_C3;
        }
    }

    Register<bit<UNIT_SIZE>, bit<32>>(FILTER_UNITS2) L2C4;

    RegisterAction<bit<UNIT_SIZE>, bit<32>, bit<UNIT_SIZE>>(L2C4) reg_update_L2_C4 = {
		void apply(inout bit<UNIT_SIZE> register_data, out bit<UNIT_SIZE> result) {
            if (hdr.myrecord.opt2 == register_data) {
                result = 0x1;
                register_data = register_data;
            } else {
                if (hdr.myrecord.bit_idx == 3) {
                    result = 0x0;
                    register_data = register_data ^ 1;
                } else {
                    result = 0x0;
                    register_data = register_data;
                }
            }
		}
	};

    action ac_update_L2_C4() {
        meta.tag_l2_c4 = reg_update_L2_C4.execute(hdr.myrecord.cnt_index2);
    }

    table tb_update_L2_C4 {
        key = {
            meta.overflow_tag_L2 : exact;
        }
        actions = {
            ac_update_L2_C4;
            NoAction;
        }
        size = 8;
        const default_action = NoAction;
        const entries = {
            1 : ac_update_L2_C4;
        }
    }

    action ac_set_overflow_tag2() {
        meta.overflow_tag_L3 = 1;
    }

    table tb_go_to_L3 {
        key = {
            meta.tag_l2_c1 : exact;
            meta.tag_l2_c2 : exact;
            meta.tag_l2_c3 : exact;
            meta.tag_l2_c4 : exact;
        }
        actions = {
            ac_set_overflow_tag2;
            NoAction;
        }
        size = 64;
        const default_action = NoAction;
        const entries = {
            (0,0,0,0) : NoAction;
            (1,0,0,0) : NoAction;
            (0,1,0,0) : NoAction;
            (0,0,1,0) : NoAction;
            (0,0,0,1) : NoAction;
            (1,1,0,0) : NoAction;
            (1,0,1,0) : NoAction;
            (1,0,0,1) : NoAction;
            (0,1,1,0) : NoAction;
            (0,1,0,1) : NoAction;
            (0,0,1,1) : NoAction;
            (1,1,1,0) : ac_set_overflow_tag2;
            (1,1,0,1) : ac_set_overflow_tag2;
            (1,0,1,1) : ac_set_overflow_tag2;
            (0,1,1,1) : ac_set_overflow_tag2;
            (1,1,1,1) : ac_set_overflow_tag2;
        }
    }

    /*
        CSE
    */

    CRCPolynomial<bit<32>>(coeff=0x11223355,reversed=true, msb=false, extended=false, init=0xFFFFFFFF, xor=0xFFFFFFFF) crc_cse;
	Hash<bit<32>>(HashAlgorithm_t.CUSTOM, crc_cse) hash_cse;

    CRCPolynomial<bit<32>>(coeff=0x88114455,reversed=true, msb=false, extended=false, init=0xFFFFFFFF, xor=0xFFFFFFFF) crc_cse_pos;
	Hash<bit<32>>(HashAlgorithm_t.CUSTOM, crc_cse_pos) hash_cse_pos;

    action ac_get_cse_func_index() {
        hdr.myrecord.func_index = hash_cse.get({hdr.ipv4.dstAddr})[7:0];
    }

    table tb_get_cse_func_index {
        key = {
            meta.overflow_tag_L3 : exact;
        }
        actions = {
            ac_get_cse_func_index;
            NoAction;
        }
        size = 8;
        const default_action = NoAction;
        const entries = {
            1 : ac_get_cse_func_index;
        }
    }

    action ac_get_cse_pos() {
        hdr.myrecord.cnt_index = (bit<32>) hash_cse_pos.get({hdr.ipv4.srcAddr, hdr.myrecord.func_index})[12:0];
    }

    table tb_get_cse_pos {
        key = {
            meta.overflow_tag_L3 : exact;
        }
        actions = {
            ac_get_cse_pos;
            NoAction;
        }
        size = 8;
        const default_action = NoAction;
        const entries = {
            1 : ac_get_cse_pos;
        }
    }

    // 256 virtual bitmap
    Register<bit<UNIT_SIZE>, bit<32>>(BIT_NUM) CSE;

    RegisterAction<bit<UNIT_SIZE>, bit<32>, bit<UNIT_SIZE>>(CSE) reg_update_cse = {
		void apply(inout bit<UNIT_SIZE> register_data, out bit<UNIT_SIZE> result) {
            register_data = 0x1;
		}
	};

    action ac_update_cse() {
        reg_update_cse.execute(hdr.myrecord.cnt_index);
    }

    table tb_update_cse {
        key = {
            meta.overflow_tag_L3 : exact;
        }
        actions = {
            ac_update_cse;
            NoAction;
        }
        const default_action = NoAction;
        const entries = {
            1 : ac_update_cse;
        }
    }

    /*
		Basic forwarding
	*/
    action drop() {
        ig_dprsr_md.drop_ctl = 1;
    }
	
    action ipv4_forward(egressSpec_t port) {
	    ig_tm_md.ucast_egress_port = port;
    }
	
	@pragma stage 0
    table ipv4_lpm {
        key = {
		    hdr.ipv4.dstAddr: lpm;
        }

        actions = {
            ipv4_forward;
            drop;
            NoAction;
        }

        size = 32;

        default_action = NoAction();
    }

    apply {
        //only if IPV4 the rule is applied. Therefore other packets will not be forwarded.
        if (hdr.ipv4.isValid()){
            tb_get_func_index.apply();
            tb_get_pos_L1.apply();
            tb_get_opt_cnt.apply();
            tb_get_bit_index1.apply();
            tb_update_L1_C1.apply();
            tb_update_L1_C2.apply();
            tb_go_to_L2.apply();
            tb_get_func_index2.apply();
            tb_get_pos_L2.apply();
            tb_get_opt_cnt2.apply();
            //tb_get_bit_index2.apply();
            tb_update_L2_C1.apply();
            tb_update_L2_C2.apply();
            tb_update_L2_C3.apply();
            tb_update_L2_C4.apply();
            tb_go_to_L3.apply();
            tb_get_cse_func_index.apply();
            tb_get_cse_pos.apply();
            tb_update_cse.apply();
			ipv4_lpm.apply();
        }
    }
}

control IngressDeparser(packet_out packet,
	inout headers hdr,
	in metadata meta,
	in ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md)
{
	apply{

		packet.emit(hdr);
	}
}
/*************************************************************************
****************  E G R E S S   P R O C E S S I N G   *******************
*************************************************************************/
parser EgressParser(packet_in packet,
	out egress_headers_t hdr,
	out egress_metadata_t meta,
	out egress_intrinsic_metadata_t eg_intr_md)
{
	state start{
		packet.extract(eg_intr_md);
		transition accept;
	}
}

control Egress(inout egress_headers_t hdr,
				inout egress_metadata_t meta,
				in egress_intrinsic_metadata_t eg_intr_md,
				in egress_intrinsic_metadata_from_parser_t eg_prsr_md,
				inout egress_intrinsic_metadata_for_deparser_t eg_dprsr_md,
				inout egress_intrinsic_metadata_for_output_port_t eg_oport_md) 
{

    apply { 
    }
}

/*************************************************************************
***********************  D E P A R S E R  *******************************
*************************************************************************/

control EgressDeparser(packet_out packet, 
						inout egress_headers_t hdr, 
						in egress_metadata_t meta, 
						in egress_intrinsic_metadata_for_deparser_t eg_dprsr_md) {
    apply {
        //parsed headers have to be added again into the packet.
		packet.emit(hdr);
    }
}

/*************************************************************************
***********************  S W I T C H  *******************************
*************************************************************************/

//switch architecture
Pipeline(
MyParser(),
MyIngress(),
IngressDeparser(),
EgressParser(),
Egress(),
EgressDeparser()
) pipe;

Switch(pipe) main;