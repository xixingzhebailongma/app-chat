#pragma once

#include <memory>
#include <string>
#include <vector>

#include "db/StudentParentRepository.h"

class PgPool;

// 基于 PostgreSQL 的 student_parents 仓库（sql/migration_v12.sql）。
// 仅在定义 HAS_LIBPQ 时编译（见 CMakeLists.txt）。
class PgStudentParentRepository : public StudentParentRepository {
public:
    explicit PgStudentParentRepository(std::shared_ptr<PgPool> pool);

    bool hasActiveStudent(const std::string& student_no) const override;
    bool matchesActiveParent(const std::string& student_no,
                             const std::string& parent_phone) const override;
    std::string findStudentName(const std::string& student_no) const override;
    std::vector<StudentParent> listAll() const override;
    void upsert(const StudentParent& row) override;
    void remove(const std::string& student_no,
                const std::string& parent_phone) override;

private:
    std::shared_ptr<PgPool> pool_;
};
