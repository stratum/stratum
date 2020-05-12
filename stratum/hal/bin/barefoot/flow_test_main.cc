
#include <bf_rt/bf_rt_info.hpp>
#include <bf_rt/bf_rt_init.hpp>
#include <bf_rt/bf_rt_common.h>
#include <bf_rt/bf_rt_table_key.hpp>
#include <bf_rt/bf_rt_table_data.hpp>
#include <bf_rt/bf_rt_table.hpp>

#include <string.h>
#include <getopt.h>

#include <chrono>
#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif
#include <bf_switchd/bf_switchd.h>
#ifdef __cplusplus
}
#endif

/***********************************************************************************
 * This sample cpp application code is based on the P4 program
 * tna_exact_match.p4
 * Please refer to the P4 program and the generated bf-rt.json for information
 * on the tables contained in the P4 program, and the associated key and data
 * fields.
 **********************************************************************************/

namespace bfrt {
namespace tna_exact_match {

namespace {
// Key field ids, table data field ids, action ids, Table object required for
// interacting with the table
const bfrt::BfRtInfo *bfrtInfo = nullptr;
const bfrt::BfRtTable *forwardTable = nullptr;
std::shared_ptr<bfrt::BfRtSession> session;

std::unique_ptr<bfrt::BfRtTableKey> bfrtTableKey;
std::unique_ptr<bfrt::BfRtTableData> bfrtTableData;

// Key field ids
bf_rt_id_t forward_ethernet_dst_addr = 0;

// Action Ids
bf_rt_id_t forward_action_hit = 0;

// Data field IDs for forward action
bf_rt_id_t forward_action_param_port = 0;


#define ALL_PIPES 0xffff
bf_rt_target_t dev_tgt;
}  // anonymous namespace

// This function does the initial setUp of getting bfrtInfo object associated
// with the P4 program from which all other required objects are obtained
void setUp() {
  dev_tgt.dev_id = 0;
  dev_tgt.pipe_id = ALL_PIPES;
  // Get devMgr singleton instance
  auto &devMgr = bfrt::BfRtDevMgr::getInstance();

  // Get bfrtInfo object from dev_id and p4 program name
  auto bf_status =
      devMgr.bfRtInfoGet(dev_tgt.dev_id, "tna_exact_match", &bfrtInfo);
  // Check for status
  assert(bf_status == BF_SUCCESS);

  // Create a session object
  session = bfrt::BfRtSession::sessionCreate();
}

// This function does the initial set up of getting key field-ids, action-ids
// and data field ids associated with the forward table. This is done once
// during init time.
void tableSetUp() {
  // Get table object from name
  auto bf_status =
      bfrtInfo->bfrtTableFromNameGet("SwitchIngress.forward", &forwardTable);
  assert(bf_status == BF_SUCCESS);

  // Get action Ids for hit and miss
  bf_status = forwardTable->actionIdGet("SwitchIngress.hit",
                                        &forward_action_hit);
  assert(bf_status == BF_SUCCESS);

  // Get field-ids for key field and data fields
  bf_status = forwardTable->keyFieldIdGet("hdr.ethernet.dst_addr",
                                          &forward_ethernet_dst_addr);
  assert(bf_status == BF_SUCCESS);


  /***********************************************************************
   * DATA FIELD ID GET FOR "hit" ACTION
   **********************************************************************/
  bf_status =
      forwardTable->dataFieldIdGet("port",
                                   forward_action_hit,
                                   &forward_action_param_port);
  assert(bf_status == BF_SUCCESS);

  // Allocate key and data once, and use reset across different uses
  bf_status = forwardTable->keyAllocate(&bfrtTableKey);
  assert(bf_status == BF_SUCCESS);

  bf_status = forwardTable->dataAllocate(&bfrtTableData);
  assert(bf_status == BF_SUCCESS);

  // Clear the table
  bf_status = forwardTable->tableClear(*session, dev_tgt);
  assert(bf_status == BF_SUCCESS);
}

void exactMatchInstallTest(int batch_size=400, int iterations=1000) {
  uint64_t dstMac = 1;
  bf_status_t bf_status = BF_SUCCESS;
  for (int i = 1; i <= iterations; i++) {
    auto t1 = std::chrono::high_resolution_clock::now();
    session->beginBatch();
    for (int b = 0; b < batch_size; b++) {
      // Reset table
      forwardTable->keyReset(bfrtTableKey.get());
      forwardTable->dataReset(forward_action_hit, bfrtTableData.get());

      // Set value into the key object. Key type is "EXACT"
      bf_status = bfrtTableKey->setValue(forward_ethernet_dst_addr, static_cast<uint64_t>(dstMac));
      assert(bf_status == BF_SUCCESS);
      dstMac++;

      // Set value into the data object
      bf_status = bfrtTableData->setValue(forward_action_param_port, static_cast<uint64_t>(1));
      assert(bf_status == BF_SUCCESS);

      // Call table entry add API
      bf_status = forwardTable->tableEntryAdd(*session, dev_tgt, *bfrtTableKey, *bfrtTableData);
      assert(bf_status == BF_SUCCESS);
    }
    session->endBatch(true);
    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();
    std::cout << i * batch_size << " " << duration << std::endl;
  }

  // Verify
  uint32_t entry_count;
  bf_status = forwardTable->tableUsageGet(*session, dev_tgt, BfRtTable::BfRtTableGetFlag::GET_FROM_HW, &entry_count);
  assert(bf_status == BF_SUCCESS);

  if (entry_count == batch_size * iterations) {
    std::cout << entry_count << " table entries installed successfully." << std::endl;
  } else {
    std::cout << "Failed, only " << entry_count << " installed." << std::endl;
  }
}

}  // tna_exact_match
}  // bfrt

static void parse_options(bf_switchd_context_t *switchd_ctx,
                          int argc,
                          char **argv) {
  int option_index = 0;
  enum opts {
    OPT_INSTALLDIR = 1,
    OPT_CONFFILE
  };
  static struct option options[] = {
      {"help", no_argument, 0, 'h'},
      {"install-dir", required_argument, 0, OPT_INSTALLDIR},
      {"conf-file", required_argument, 0, OPT_CONFFILE}
  };

  while (1) {
    int c = getopt_long(argc, argv, "h", options, &option_index);

    if (c == -1) {
      break;
    }
    switch (c) {
      case OPT_INSTALLDIR:
        switchd_ctx->install_dir = strdup(optarg);
        std::cout << "Install Dir: " << switchd_ctx->install_dir << std::endl;
        break;
      case OPT_CONFFILE:
        switchd_ctx->conf_file = strdup(optarg);
        std::cout << "Conf-file : " << switchd_ctx->conf_file << std::endl;
        break;
      case 'h':
      case '?':
        std::cout << "tna_exact_match" << std::endl;
        std::cout <<
            "Usage : tna_exact_match --install-dir <path to where the SDE is "
            "installed> --conf-file <full path to the conf file "
            "(tna_exact_match.conf)" << std::endl;
        exit(c == 'h' ? 0 : 1);
        break;
      default:
        std::cout << "Invalid option" << std::endl;
        exit(0);
        break;
    }
  }
  if (switchd_ctx->install_dir == NULL) {
    std::cout << "ERROR : --install-dir must be specified" << std::endl;
    exit(0);
  }

  if (switchd_ctx->conf_file == NULL) {
    std::cout << "ERROR : --conf-file must be specified" << std::endl;
    exit(0);
  }
}

int main(int argc, char **argv) {
  bf_switchd_context_t *switchd_ctx;
  if ((switchd_ctx = (bf_switchd_context_t *)calloc(
           1, sizeof(bf_switchd_context_t))) == NULL) {
    std::cout << "Cannot Allocate switchd context" << std::endl;
    exit(1);
  }
  parse_options(switchd_ctx, argc, argv);
  switchd_ctx->running_in_background = true;
  bf_status_t status = bf_switchd_lib_init(switchd_ctx);

  // Do initial set up
  bfrt::tna_exact_match::setUp();
  // Do table level set up
  bfrt::tna_exact_match::tableSetUp();
  // Test!
  int iterations = atoi(argv[argc - 1]);
  int batch_size = atoi(argv[argc - 2]);
  bfrt::tna_exact_match::exactMatchInstallTest(batch_size, iterations);
  std::cout << "DONE!" << std::endl;
  return status;
}
