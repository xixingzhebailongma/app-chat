#pragma once

#include <memory>
#include <string>
#include <vector>

#include "db/ParentBindingRepository.h"

class PgPool;

// 基于 PostgreSQL 的 parent_student_bindings 仓库（sql/migration_v1.sql）。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgParentBindingRepository : public ParentBindingRepository {
public:
    explicit PgParentBindingRepository(std::shared_ptr<PgPool> pool);

    std::vector<std::string> findParentOpenidsByStudentNo(
        const std::string& student_no) const override;
    std::vector<ParentBinding> findBindingsByOpenid(
        const std::string& parent_openid_oa) const override;
    void save(const ParentBinding& binding) override;
    bool remove(const std::string& parent_openid_oa,
                const std::string& student_no) override;
    void updateReachStatus(const std::string& parent_openid_oa,
                           const std::string& student_no,
                           const std::string& status) override;

private:
    std::shared_ptr<PgPool> pool_;
};
