# Stratum P4 Role Configuration

Stratum supports the [P4Runtime Role Configuration](https://p4.org/p4-spec/p4runtime/main/P4Runtime-Spec.html#sec-arbitration-role-config)
feature, with a similar implementation set as described in the specification.

Stratum's role config description is protobuf based and can be found under
[stratum/public/proto/p4_role_config.proto](/stratum/public/proto/p4_role_config.proto).
In particular, it allows a given role to:
- restrict r/w access to a subset of P4 entities, identified by their ID
- enable or disable receipt of `PacketIn` messages
- filter PacketIn messages based on their `PacketMetadata`
- enable or disable ability to push a new pipeline


TODO: `any` instructions. Packing, url and bytes.
