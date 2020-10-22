
import oc_helpers


def main():
    # General setup
    oc_helpers.setup_drop_profiles()
    oc_helpers.setup_queues()
    oc_helpers.setup_interfaces()
    oc_helpers.setup_schedulers("if1")

    # Setup HQoS on if1
    oc_helpers.add_hqos("if1")

    # Print 
    oc_helpers.print_qos()


if __name__ == "__main__":
    main()
