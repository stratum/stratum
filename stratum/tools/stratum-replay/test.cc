#include <iostream>
#include <regex>
#include <string>

void foo() {
  std::string line =
    "2020-10-19 14:15:36.978545;1;type: INSERT entity { table_entry { table_id: 33577058 match { field_id: 1 exact { value: \"\\000\\000\\000\\002\" } } action { action_profile_group_id: 2 } } };'table->tableEntryAdd(*bfrt_session, bf_dev_tgt, *table_key, *table_data)' failed with error message: Object not found. Failed to insert table entry table_id: 33577058 match { field_id: 1 exact { value: \"\\000\\000\\000\\002\" } } action { action_profile_group_id: 2 }.";
  // [something];[proto message];[something random text (can be empty)]
  std::regex re(";(type[^;]*);");
  auto b = std::sregex_iterator(line.begin(), line.end(), re);
  auto e = std::sregex_iterator();

  std::cout << "Find " << std::distance(b, e) << std::endl;
  for (auto i = b; i != e; i++) {
    std::smatch match = *i;
    std::string str = match.str();
    std::cout << "Full: " << str << std::endl;
    for (int c = 0; c < match.size(); c++) {
      std::cout << "Sub[" << c << "]: " << match[c].str() << std::endl;
    }

  }
}

int main() {
  foo();
  return 0;
}