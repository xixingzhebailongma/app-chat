#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

// student_parents 表的一行（sql/migration_v12.sql，设计文档 7.8 §7）。
// 一行一家长：一个学生可多行（父母双方等），(student_no, parent_phone) 唯一。
struct StudentParent {
    std::string student_no;
    std::string student_name;
    std::string parent_phone;
    std::string relation = "其他";   // 父亲/母亲/其他
    std::string status = "active";   // active / inactive
};

// 学生花名册（学校权威数据）。绑定流程用它做校验（学号存在 + 手机号匹配），
// 全量同步接口 /internal/student-parents/sync 用它做对账。
class StudentParentRepository {
public:
    virtual ~StudentParentRepository() = default;

    // 学号存在且 status=active。
    virtual bool hasActiveStudent(const std::string& student_no) const = 0;

    // (student_no, parent_phone) 存在且 status=active。
    virtual bool matchesActiveParent(const std::string& student_no,
                                     const std::string& parent_phone) const = 0;

    // 学号对应的学生姓名（join 花名册，缺失返回空串）。
    virtual std::string findStudentName(const std::string& student_no) const = 0;

    // 全量列表（供同步对账计算差集）。
    virtual std::vector<StudentParent> listAll() const = 0;

    // 幂等写入（不存在则插，存在则更新 name/relation/status）。
    virtual void upsert(const StudentParent& row) = 0;

    // 删除一行（幂等）。
    virtual void remove(const std::string& student_no,
                        const std::string& parent_phone) = 0;
};

// 内存 mock；真实落库见 PgStudentParentRepository。开发装配 / 测试通过 add() 播种。
class InMemoryStudentParentRepository : public StudentParentRepository {
public:
    void add(const StudentParent& row) {
        byKey_[{row.student_no, row.parent_phone}] = row;
    }

    bool hasActiveStudent(const std::string& student_no) const override {
        for (const auto& [key, row] : byKey_) {
            if (key.first == student_no && row.status == "active") {
                return true;
            }
        }
        return false;
    }

    bool matchesActiveParent(const std::string& student_no,
                             const std::string& parent_phone) const override {
        const auto it = byKey_.find({student_no, parent_phone});
        return it != byKey_.end() && it->second.status == "active";
    }

    std::string findStudentName(const std::string& student_no) const override {
        for (const auto& [key, row] : byKey_) {
            if (key.first == student_no && row.status == "active") {
                return row.student_name;
            }
        }
        return "";
    }

    std::vector<StudentParent> listAll() const override {
        std::vector<StudentParent> out;
        out.reserve(byKey_.size());
        for (const auto& [key, row] : byKey_) {
            (void)key;
            out.push_back(row);
        }
        return out;
    }

    void upsert(const StudentParent& row) override {
        byKey_[{row.student_no, row.parent_phone}] = row;
    }

    void remove(const std::string& student_no,
                const std::string& parent_phone) override {
        byKey_.erase({student_no, parent_phone});
    }

private:
    std::map<std::pair<std::string, std::string>, StudentParent> byKey_;
};
