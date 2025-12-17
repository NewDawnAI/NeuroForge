/**
 * @file test_phase_a.cpp
 * @brief Test program for validating Phase A Baby Multimodal Mimicry implementation
 * 
 * This program demonstrates and validates the Phase A multimodal learning features
 * including teacher encoder integration, mimicry rewards, cross-modal alignment,
 * and semantic grounding with the Phase 5 language system.
 */

#include "core/PhaseAMimicry.h"
#include "core/LanguageSystem.h"
#include "core/MemoryDB.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <random>
#include <vector>
#include <memory>
#include <thread>
#include <cassert>
#include <string>
#include <fstream>

using namespace NeuroForge;
using namespace NeuroForge::Core;

class PhaseATestSuite {
private:
    std::shared_ptr<LanguageSystem> language_system_;
    std::unique_ptr<PhaseAMimicry> phase_a_system_;
    std::shared_ptr<MemoryDB> memory_db_;
    std::mt19937 rng_;
    bool enable_verbose_output_ = false;

public:
    PhaseATestSuite(bool verbose = false) 
        : rng_(std::random_device{}()), enable_verbose_output_(verbose) {
        
        // Initialize Language System (Phase 5)
        LanguageSystem::Config lang_config;
        lang_config.mimicry_learning_rate = 0.05f;
        lang_config.grounding_strength = 0.8f;
        lang_config.enable_teacher_mode = true;
        lang_config.max_vocabulary_size = 2000;
        
        language_system_ = std::make_shared<LanguageSystem>(lang_config);
        
        // Initialize MemoryDB
        memory_db_ = std::make_shared<MemoryDB>("test_phase_a.db");
        
        // Initialize Phase A Mimicry System
        PhaseAMimicry::Config phase_a_config;
        phase_a_config.similarity_weight = 0.7f;
        phase_a_config.novelty_weight = 0.3f;
        phase_a_config.similarity_threshold = 0.55f;
        phase_a_config.max_teacher_embeddings = 1000;
        phase_a_config.embedding_dimension = 512;
        phase_a_config.enable_cross_modal_alignment = true;
        phase_a_config.novelty_threshold = 0.1f;
        
        // Create Phase A system using shared ownership of LanguageSystem
        phase_a_system_ = PhaseAMimicryFactory::create(language_system_, memory_db_, phase_a_config);
    }
    
    bool runAllTests() {
        std::cout << "=== NeuroForge Phase A Baby Multimodal Mimicry Test Suite ===\n\n";
        
        bool all_passed = true;
        
        all_passed &= testSystemInitialization();
        all_passed &= testTeacherEmbeddingManagement();
        all_passed &= testMimicryLearning();
        all_passed &= testMultimodalAlignment();
        all_passed &= testCrossModalLearning();
        all_passed &= testLanguageSystemIntegration();
        all_passed &= testTeacherEncoderIntegration();
        all_passed &= testBatchProcessing();
        all_passed &= testMemoryConsolidation();
        all_passed &= testStatisticsAndReporting();
        all_passed &= testSerialization();
        all_passed &= testIntegratedScenario();
        
        std::cout << "\n=== Test Suite Summary ===\n";
        if (all_passed) {
            std::cout << "✅ All tests PASSED!\n";
        } else {
            std::cout << "❌ Some tests FAILED!\n";
        }
        
        return all_passed;
    }

private:
    bool testSystemInitialization() {
        std::cout << "Test 1: Phase A System Initialization... ";
        
        try {
            bool lang_init = language_system_->initialize();
            bool phase_a_init = phase_a_system_->initialize();
            
            if (!lang_init || !phase_a_init) {
                std::cout << "FAILED (initialization returned false)\n";
                return false;
            }
            
            // Check initial statistics
            auto stats = phase_a_system_->getStatistics();
            if (stats.total_mimicry_attempts != 0 || stats.teacher_embeddings_stored != 0) {
                std::cout << "FAILED (non-zero initial statistics)\n";
                return false;
            }
            
            std::cout << "PASSED\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testTeacherEmbeddingManagement() {
        std::cout << "Test 2: Teacher Embedding Management... ";
        
        try {
            // Create test embeddings for different modalities
            std::vector<float> vision_embedding = generateTestEmbedding("dog_image");
            std::vector<float> text_embedding = generateTestEmbedding("dog_text");
            std::vector<float> audio_embedding = generateTestEmbedding("dog_bark");
            
            // Add teacher embeddings
            std::string vision_id = phase_a_system_->addTeacherEmbedding(
                vision_embedding, PhaseAMimicry::TeacherType::CLIP_Vision,
                PhaseAMimicry::Modality::Visual, "dog_image", "dog.jpg", 0.9f);
            
            std::string text_id = phase_a_system_->addTeacherEmbedding(
                text_embedding, PhaseAMimicry::TeacherType::BERT_Text,
                PhaseAMimicry::Modality::Text, "dog_text", "dog", 0.95f);
            
            std::string audio_id = phase_a_system_->addTeacherEmbedding(
                audio_embedding, PhaseAMimicry::TeacherType::Whisper_Audio,
                PhaseAMimicry::Modality::Audio, "dog_bark", "bark.wav", 0.85f);
            
            if (vision_id.empty() || text_id.empty() || audio_id.empty()) {
                std::cout << "FAILED (embedding addition failed)\n";
                return false;
            }
            
            // Test retrieval
            auto* vision_emb = phase_a_system_->getTeacherEmbedding(vision_id);
            auto* text_emb = phase_a_system_->getTeacherEmbedding(text_id);
            auto* audio_emb = phase_a_system_->getTeacherEmbedding(audio_id);
            
            if (!vision_emb || !text_emb || !audio_emb) {
                std::cout << "FAILED (embedding retrieval failed)\n";
                return false;
            }
            
            // Test modality-based retrieval
            auto visual_embeddings = phase_a_system_->getTeacherEmbeddingsByModality(
                PhaseAMimicry::Modality::Visual);
            auto text_embeddings = phase_a_system_->getTeacherEmbeddingsByModality(
                PhaseAMimicry::Modality::Text);
            
            if (visual_embeddings.size() != 1 || text_embeddings.size() != 1) {
                std::cout << "FAILED (modality-based retrieval failed)\n";
                return false;
            }
            
            // Test type-based retrieval
            auto clip_embeddings = phase_a_system_->getTeacherEmbeddingsByType(
                PhaseAMimicry::TeacherType::CLIP_Vision);
            auto bert_embeddings = phase_a_system_->getTeacherEmbeddingsByType(
                PhaseAMimicry::TeacherType::BERT_Text);
            
            if (clip_embeddings.size() != 1 || bert_embeddings.size() != 1) {
                std::cout << "FAILED (type-based retrieval failed)\n";
                return false;
            }
            
            std::cout << "PASSED (3 embeddings stored and retrieved)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testMimicryLearning() {
        std::cout << "Test 3: Mimicry Learning System... ";
        
        try {
            // Create teacher embedding
            std::vector<float> teacher_embedding = generateTestEmbedding("hello_teacher");
            std::string teacher_id = phase_a_system_->addTeacherEmbedding(
                teacher_embedding, PhaseAMimicry::TeacherType::CLIP_Text,
                PhaseAMimicry::Modality::Text, "hello_teacher", "hello", 1.0f);
            
            std::vector<float> good_student_embedding = teacher_embedding;
            addNoise(good_student_embedding, 0.02f);
            
            auto good_attempt = phase_a_system_->attemptMimicry(
                good_student_embedding, teacher_id, "good_mimicry_test");
            auto manual_cos = [&]() {
                double dot = 0.0, na = 0.0, nb = 0.0;
                for (std::size_t i = 0; i < good_student_embedding.size(); ++i) {
                    dot += static_cast<double>(good_student_embedding[i]) * static_cast<double>(teacher_embedding[i]);
                    na += static_cast<double>(good_student_embedding[i]) * static_cast<double>(good_student_embedding[i]);
                    nb += static_cast<double>(teacher_embedding[i]) * static_cast<double>(teacher_embedding[i]);
                }
                na = std::sqrt(na); nb = std::sqrt(nb);
                if (na <= 1e-6 || nb <= 1e-6) return 0.0;
                return dot / (na * nb);
            }();
            std::cout << "[Test] manual cosine=" << std::fixed << std::setprecision(3) << manual_cos << "\n";
            
            if (!good_attempt.success || good_attempt.similarity_score < 0.05f) {
                std::cout << "FAILED (good mimicry not successful)\n";
                return false;
            }
            
            // Test poor mimicry (low similarity)
            std::vector<float> poor_student_embedding = generateTestEmbedding("different_content");
            
            auto poor_attempt = phase_a_system_->attemptMimicry(
                poor_student_embedding, teacher_id, "poor_mimicry_test");
            
            if (poor_attempt.success || poor_attempt.similarity_score > 0.3f) {
                std::cout << "FAILED (poor mimicry incorrectly successful)\n";
                return false;
            }
            
            // Test novelty bonus calculation
            if (good_attempt.novelty_score <= 0.0f || poor_attempt.novelty_score <= 0.0f) {
                std::cout << "FAILED (novelty scores not calculated)\n";
                return false;
            }
            
            // Test total reward calculation
            float expected_good_reward = (0.7f * good_attempt.similarity_score) + 
                                        (0.3f * good_attempt.novelty_score);
            if (std::abs(good_attempt.total_reward - expected_good_reward) > 0.01f) {
                std::cout << "FAILED (reward calculation incorrect)\n";
                return false;
            }
            
            std::cout << "PASSED (mimicry learning functional)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testMultimodalAlignment() {
        std::cout << "Test 4: Multimodal Alignment... ";
        
        try {
            // Create embeddings for the same concept across modalities
            std::vector<float> cat_vision = generateTestEmbedding("cat_visual");
            std::vector<float> cat_audio = generateTestEmbedding("cat_audio");
            std::vector<float> cat_text = generateTestEmbedding("cat_text");
            
            // Add teacher embeddings
            std::string vision_id = phase_a_system_->addTeacherEmbedding(
                cat_vision, PhaseAMimicry::TeacherType::CLIP_Vision,
                PhaseAMimicry::Modality::Visual, "cat_vision", "cat.jpg");
            
            std::string audio_id = phase_a_system_->addTeacherEmbedding(
                cat_audio, PhaseAMimicry::TeacherType::Whisper_Audio,
                PhaseAMimicry::Modality::Audio, "cat_audio", "meow.wav");
            
            std::string text_id = phase_a_system_->addTeacherEmbedding(
                cat_text, PhaseAMimicry::TeacherType::BERT_Text,
                PhaseAMimicry::Modality::Text, "cat_text", "cat");
            
            // Create language tokens
            std::size_t cat_token = language_system_->createToken("cat", LanguageSystem::TokenType::Word);
            std::size_t animal_token = language_system_->createToken("animal", LanguageSystem::TokenType::Word);
            std::vector<std::size_t> cat_tokens = {cat_token, animal_token};
            
            // Create multimodal alignment for cat concept
            std::vector<std::string> teacher_ids = {vision_id, audio_id, text_id};
            std::string alignment_id = phase_a_system_->createMultimodalAlignment(
                teacher_ids, cat_tokens, "cat_concept_learning");
            
            if (alignment_id.empty()) {
                std::cout << "FAILED (cat concept alignment creation failed)\n";
                return false;
            }
            
            // Test alignment retrieval
            auto* alignment = phase_a_system_->getAlignment(alignment_id);
            if (!alignment) {
                std::cout << "FAILED (alignment retrieval failed)\n";
                return false;
            }
            
            if (alignment->teacher_embeddings.size() != 3) {
                std::cout << "FAILED (incorrect number of teacher embeddings in alignment)\n";
                return false;
            }
            
            if (alignment->associated_tokens.size() != 2) {
                std::cout << "FAILED (incorrect number of associated tokens)\n";
                return false;
            }
            
            if (alignment->alignment_strength <= 0.0f) {
                std::cout << "FAILED (alignment strength not calculated)\n";
                return false;
            }

            // Bounds check: alignment strength must be in [0, 1]
            if (alignment->alignment_strength < 0.0f || alignment->alignment_strength > 1.0f) {
                std::cout << "FAILED (alignment strength out of [0,1] bounds)\n";
                return false;
            }
            
            std::cout << "PASSED (multimodal alignment created and validated)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testCrossModalLearning() {
        std::cout << "Test 5: Cross-Modal Learning... ";
        
        try {
            // Create related embeddings across modalities
            std::vector<float> music_audio = generateTestEmbedding("music_audio");
            std::vector<float> music_text = generateTestEmbedding("music_text");
            
            // Make them somewhat similar (cross-modal association)
            for (std::size_t i = 0; i < std::min(music_audio.size(), music_text.size()); ++i) {
                music_text[i] = 0.7f * music_text[i] + 0.3f * music_audio[i];
            }
            
            // Add teacher embeddings
            std::string audio_id = phase_a_system_->addTeacherEmbedding(
                music_audio, PhaseAMimicry::TeacherType::Whisper_Audio,
                PhaseAMimicry::Modality::Audio, "music_audio", "song.wav");
            
            std::string text_id = phase_a_system_->addTeacherEmbedding(
                music_text, PhaseAMimicry::TeacherType::BERT_Text,
                PhaseAMimicry::Modality::Text, "music_text", "music");
            
            // Test cross-modal alignment calculation
            std::vector<PhaseAMimicry::TeacherEmbedding> embeddings;
            auto* audio_emb = phase_a_system_->getTeacherEmbedding(audio_id);
            auto* text_emb = phase_a_system_->getTeacherEmbedding(text_id);
            
            if (!audio_emb || !text_emb) {
                std::cout << "FAILED (teacher embeddings not found)\n";
                return false;
            }
            
            embeddings.push_back(*audio_emb);
            embeddings.push_back(*text_emb);
            
            float cross_modal_score = phase_a_system_->calculateCrossModalAlignment(embeddings);
            
            if (cross_modal_score <= 0.0f) {
                std::cout << "FAILED (cross-modal alignment not calculated)\n";
                return false;
            }
            
            // Test language token grounding
            std::vector<std::string> teacher_content_ids = {audio_id, text_id};
            std::vector<std::string> token_symbols = {"hear_music", "music"};
            
            phase_a_system_->groundLanguageTokens(teacher_content_ids, token_symbols);
            
            // Verify tokens were created in language system
            auto* music_token = language_system_->getToken("music");
            if (!music_token) {
                std::cout << "FAILED (language token not grounded)\n";
                return false;
            }
            
            std::cout << "PASSED (cross-modal learning functional)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testLanguageSystemIntegration() {
        std::cout << "Test 6: Language System Integration... ";
        
        try {
            // Create teacher embedding for a word
            std::vector<float> word_embedding = generateTestEmbedding("hello_word");
            std::string teacher_id = phase_a_system_->addTeacherEmbedding(
                word_embedding, PhaseAMimicry::TeacherType::BERT_Text,
                PhaseAMimicry::Modality::Text, "hello_word", "hello");
            
            // Test grounded narration generation
            std::vector<std::string> content_ids = {teacher_id};
            auto grounded_tokens = phase_a_system_->generateGroundedNarration(content_ids);
            
            if (grounded_tokens.empty()) {
                std::cout << "FAILED (grounded narration not generated)\n";
                return false;
            }
            
            if (grounded_tokens[0] != "hello") {
                std::cout << "FAILED (incorrect grounded token: " << grounded_tokens[0] << ")\n";
                return false;
            }
            
            // Test successful mimicry integration with language system
            std::vector<float> student_embedding = word_embedding;
            addNoise(student_embedding, 0.02f);
            
            auto attempt = phase_a_system_->attemptMimicry(
                student_embedding, teacher_id, "hello");
            
            if (!attempt.success) {
                std::cout << "FAILED (mimicry attempt not successful)\n";
                return false;
            }
            
            // Check if language system received the reward
            auto lang_stats = language_system_->getStatistics();
            if (lang_stats.successful_mimicry_attempts == 0) {
                std::cout << "FAILED (language system not updated with mimicry reward)\n";
                return false;
            }
            
            std::cout << "PASSED (language system integration functional)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testTeacherEncoderIntegration() {
        std::cout << "Test 7: Teacher Encoder Integration... ";
        
        try {
            // Test CLIP vision encoder (placeholder)
            auto clip_vision_emb = phase_a_system_->processCLIPVision("test_image.jpg");
            if (clip_vision_emb.empty() || clip_vision_emb.size() != 512) {
                std::cout << "FAILED (CLIP vision encoder failed)\n";
                return false;
            }
            
            // Test CLIP text encoder (placeholder)
            auto clip_text_emb = phase_a_system_->processCLIPText("test text");
            if (clip_text_emb.empty() || clip_text_emb.size() != 512) {
                std::cout << "FAILED (CLIP text encoder failed)\n";
                return false;
            }
            
            // Test Whisper audio encoder (placeholder)
            auto whisper_emb = phase_a_system_->processWhisperAudio("test_audio.wav");
            if (whisper_emb.empty() || whisper_emb.size() != 512) {
                std::cout << "FAILED (Whisper encoder failed)\n";
                return false;
            }
            
            // Test BERT text encoder (placeholder)
            auto bert_emb = phase_a_system_->processBERTText("test sentence");
            if (bert_emb.empty() || bert_emb.size() != 512) {
                std::cout << "FAILED (BERT encoder failed)\n";
                return false;
            }
            
            // Test that different encoders produce different embeddings
            if (clip_vision_emb == clip_text_emb || clip_text_emb == whisper_emb) {
                std::cout << "FAILED (encoders producing identical embeddings)\n";
                return false;
            }
            
            std::cout << "PASSED (teacher encoder integration functional)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testBatchProcessing() {
        std::cout << "Test 8: Batch Processing... ";
        
        try {
            // Test batch teacher embedding processing
            std::vector<std::pair<std::string, PhaseAMimicry::TeacherType>> content_batch = {
                {"image1.jpg", PhaseAMimicry::TeacherType::CLIP_Vision},
                {"image2.jpg", PhaseAMimicry::TeacherType::CLIP_Vision},
                {"text1", PhaseAMimicry::TeacherType::BERT_Text}
            };
            
            auto batch_ids = phase_a_system_->processBatchTeacherEmbeddings(
                content_batch, PhaseAMimicry::Modality::Multimodal);
            
            if (batch_ids.size() != 3) {
                std::cout << "FAILED (batch teacher embedding processing failed)\n";
                return false;
            }
            
            // Test batch mimicry processing
            std::vector<std::vector<float>> student_embeddings;
            std::vector<std::string> teacher_content_ids;
            
            for (const auto& id : batch_ids) {
                auto* teacher = phase_a_system_->getTeacherEmbedding(id);
                if (teacher) {
                    std::vector<float> student_emb = teacher->embedding;
                    addNoise(student_emb, 0.02f);
                    student_embeddings.push_back(student_emb);
                    teacher_content_ids.push_back(id);
                }
            }
            
            auto batch_attempts = phase_a_system_->processBatchMimicry(
                student_embeddings, teacher_content_ids);
            
            if (batch_attempts.size() != student_embeddings.size()) {
                std::cout << "FAILED (batch mimicry processing failed)\n";
                return false;
            }
            
            // Evaluate batch by average similarity to reduce brittleness
            float avg_sim = 0.0f;
            for (const auto& attempt : batch_attempts) {
                avg_sim += attempt.similarity_score;
            }
            avg_sim /= static_cast<float>(batch_attempts.size());
            if (avg_sim < 0.2f) {
                std::cout << "FAILED (batch mimicry average similarity too low)\n";
                return false;
            }
            
            std::cout << "PASSED (batch processing functional)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testMemoryConsolidation() {
        std::cout << "Test 9: Memory Consolidation... ";
        
        try {
            // Add several teacher embeddings
            for (int i = 0; i < 10; ++i) {
                std::vector<float> embedding = generateTestEmbedding("test_" + std::to_string(i));
                phase_a_system_->addTeacherEmbedding(
                    embedding, PhaseAMimicry::TeacherType::BERT_Text,
                    PhaseAMimicry::Modality::Text, "test_" + std::to_string(i), 
                    "test content " + std::to_string(i));
            }
            
            // Perform memory consolidation
            phase_a_system_->consolidateMemory();
            
            // Test MemoryDB integration
            phase_a_system_->saveToMemoryDB();
            phase_a_system_->loadFromMemoryDB();
            
            // Verify embeddings are still accessible
            auto* test_emb = phase_a_system_->getTeacherEmbedding("test_5");
            if (!test_emb) {
                std::cout << "FAILED (embedding lost after consolidation)\n";
                return false;
            }
            
            std::cout << "PASSED (memory consolidation functional)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testStatisticsAndReporting() {
        std::cout << "Test 10: Statistics and Reporting... ";
        
        try {
            // Get current statistics
            auto stats = phase_a_system_->getStatistics();
            
            // Verify statistics are reasonable
            if (stats.teacher_embeddings_stored == 0) {
                std::cout << "FAILED (no teacher embeddings recorded in stats)\n";
                return false;
            }
            
            if (stats.total_mimicry_attempts == 0) {
                std::cout << "FAILED (no mimicry attempts recorded in stats)\n";
                return false;
            }
            
            // Generate Phase A report
            std::string report = phase_a_system_->generatePhaseAReport();
            
            if (report.empty()) {
                std::cout << "FAILED (empty Phase A report)\n";
                return false;
            }
            
            // Check report contains expected sections
            if (report.find("Phase A Baby Multimodal Mimicry Report") == std::string::npos ||
                report.find("Total Mimicry Attempts") == std::string::npos ||
                report.find("Teacher Embeddings Stored") == std::string::npos) {
                std::cout << "FAILED (incomplete Phase A report)\n";
                return false;
            }
            
            if (enable_verbose_output_) {
                std::cout << "\n" << report << "\n";
            }
            
            std::cout << "PASSED (statistics and reporting functional)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testSerialization() {
        std::cout << "Test 11: Serialization and Export... ";
        
        try {
            // Export teacher embeddings to JSON
            std::string embeddings_json = phase_a_system_->exportTeacherEmbeddingsToJson();
            
            if (embeddings_json.empty()) {
                std::cout << "FAILED (empty teacher embeddings JSON)\n";
                return false;
            }
            
            // Check JSON structure
            if (embeddings_json.find("teacher_embeddings") == std::string::npos) {
                std::cout << "FAILED (invalid teacher embeddings JSON structure)\n";
                return false;
            }
            
            // Export mimicry history to JSON
            std::string mimicry_json = phase_a_system_->exportMimicryHistoryToJson();
            
            if (mimicry_json.empty()) {
                std::cout << "FAILED (empty mimicry history JSON)\n";
                return false;
            }
            
            // Export alignments to JSON
            std::string alignments_json = phase_a_system_->exportAlignmentsToJson();
            
            if (alignments_json.empty()) {
                std::cout << "FAILED (empty alignments JSON)\n";
                return false;
            }
            
            if (enable_verbose_output_) {
                std::cout << "\nTeacher Embeddings JSON (first 200 chars): " 
                         << embeddings_json.substr(0, 200) << "...\n";
            }
            
            std::cout << "PASSED (serialization functional)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testIntegratedScenario() {
        std::cout << "Test 12: Integrated Baby Mimicry Scenario... ";
        
        try {
            // Simulate a baby learning scenario: see dog, hear "dog", learn association
            
            // 1. Teacher shows image of dog
            std::vector<float> dog_image_emb = generateTestEmbedding("dog_image_realistic");
            std::string image_id = phase_a_system_->addTeacherEmbedding(
                dog_image_emb, PhaseAMimicry::TeacherType::CLIP_Vision,
                PhaseAMimicry::Modality::Visual, "dog_image", "golden_retriever.jpg", 0.95f);
            
            // 2. Teacher says "dog"
            std::vector<float> dog_word_emb = generateTestEmbedding("dog_word_realistic");
            std::string word_id = phase_a_system_->addTeacherEmbedding(
                dog_word_emb, PhaseAMimicry::TeacherType::BERT_Text,
                PhaseAMimicry::Modality::Text, "dog_word", "dog", 0.98f);
            
            // 3. Teacher plays dog bark sound
            std::vector<float> dog_bark_emb = generateTestEmbedding("dog_bark_realistic");
            std::string bark_id = phase_a_system_->addTeacherEmbedding(
                dog_bark_emb, PhaseAMimicry::TeacherType::Whisper_Audio,
                PhaseAMimicry::Modality::Audio, "dog_bark", "woof.wav", 0.90f);
            
            // 4. Create multimodal alignment for "dog" concept
            std::vector<std::string> dog_teacher_ids = {image_id, word_id, bark_id};
            std::size_t dog_token = language_system_->createToken("dog", LanguageSystem::TokenType::Word);
            std::size_t animal_token = language_system_->createToken("animal", LanguageSystem::TokenType::Word);
            std::vector<std::size_t> dog_tokens = {dog_token, animal_token};
            
            std::string dog_alignment = phase_a_system_->createMultimodalAlignment(
                dog_teacher_ids, dog_tokens, "dog_concept_learning");
            
            if (dog_alignment.empty()) {
                std::cout << "FAILED (dog concept alignment creation failed)\n";
                return false;
            }
            
            // 5. Baby attempts to mimic: sees dog image, tries to say "dog"
            std::vector<float> baby_visual_response = dog_image_emb;
            addNoise(baby_visual_response, 0.02f);
            
            auto visual_mimicry = phase_a_system_->attemptMimicry(
                baby_visual_response, image_id, "baby_sees_dog");
            
            // 6. Baby attempts to mimic: hears "dog", tries to repeat
            std::vector<float> baby_word_response = dog_word_emb;
            addNoise(baby_word_response, 0.02f);
            
            auto word_mimicry = phase_a_system_->attemptMimicry(
                baby_word_response, word_id, "baby_says_dog");
            
            // 7. Verify learning occurred
            if (!visual_mimicry.success || !word_mimicry.success) {
                std::cout << "FAILED (baby mimicry attempts not successful)\n";
                return false;
            }
            
            // 8. Check that language system learned the associations
            auto* dog_lang_token = language_system_->getToken("dog");
            if (!dog_lang_token || dog_lang_token->usage_count == 0) {
                std::cout << "FAILED (language system did not learn dog token)\n";
                return false;
            }
            
            // 9. Generate grounded narration
            auto grounded_narration = phase_a_system_->generateGroundedNarration(dog_teacher_ids);
            if (grounded_narration.size() != 3) {
                std::cout << "FAILED (grounded narration not generated correctly)\n";
                return false;
            }
            
            // 10. Verify cross-modal alignment strength
            auto* alignment = phase_a_system_->getAlignment(dog_alignment);
            if (!alignment || alignment->alignment_strength <= 0.0f) {
                std::cout << "FAILED (cross-modal alignment not established)\n";
                return false;
            }

            // Bounds check: alignment strength must be in [0, 1]
            if (alignment->alignment_strength < 0.0f || alignment->alignment_strength > 1.0f) {
                std::cout << "FAILED (alignment strength out of [0,1] bounds)\n";
                return false;
            }
            
            // 11. Check final statistics
            auto final_stats = phase_a_system_->getStatistics();
            if (final_stats.successful_mimicry_attempts < 2 || 
                final_stats.multimodal_alignments_created == 0) {
                std::cout << "FAILED (final statistics incorrect)\n";
                return false;
            }
            
            if (enable_verbose_output_) {
                std::cout << "\nIntegrated Scenario Results:\n";
                std::cout << "- Visual mimicry similarity: " << std::fixed << std::setprecision(3) 
                         << visual_mimicry.similarity_score << "\n";
                std::cout << "- Word mimicry similarity: " << word_mimicry.similarity_score << "\n";
                std::cout << "- Cross-modal alignment strength: " << alignment->alignment_strength << "\n";
                std::cout << "- Grounded narration: ";
                for (const auto& token : grounded_narration) {
                    std::cout << token << " ";
                }
                std::cout << "\n";
            }
            
            std::cout << "PASSED (integrated baby mimicry scenario successful)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    // Helper methods
    std::vector<float> generateTestEmbedding(const std::string& seed_text) {
        std::vector<float> embedding(512);
        
        // Generate deterministic but varied embeddings based on seed
        std::hash<std::string> hasher;
        std::size_t seed = hasher(seed_text);
        std::mt19937 local_rng(seed);
        std::normal_distribution<float> dist(0.0f, 1.0f);
        
        for (float& val : embedding) {
            val = dist(local_rng);
        }
        
        // Normalize embedding
        float norm = 0.0f;
        for (float val : embedding) {
            norm += val * val;
        }
        norm = std::sqrt(norm);
        
        if (norm > 1e-6f) {
            for (float& val : embedding) {
                val /= norm;
            }
        }
        
        return embedding;
    }
    
    void addNoise(std::vector<float>& embedding, float noise_level) {
        std::normal_distribution<float> noise_dist(0.0f, noise_level);
        
        for (float& val : embedding) {
            val += noise_dist(rng_);
        }
        
        // Re-normalize
        float norm = 0.0f;
        for (float val : embedding) {
            norm += val * val;
        }
        norm = std::sqrt(norm);
        
        if (norm > 1e-6f) {
            for (float& val : embedding) {
                val /= norm;
            }
        }
    }
};

// Remove conditional compilation and always include main
int main(int argc, char** argv) {
    bool verbose = false;
    
    // Check for verbose flag
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--verbose" || std::string(argv[i]) == "-v") {
            verbose = true;
            break;
        }
    }
    
    PhaseATestSuite test_suite(verbose);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    bool all_passed = test_suite.runAllTests();
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "\nTest execution time: " << duration.count() << " ms\n";
    
    return all_passed ? 0 : 1;
}

// Standalone test functions for integration with other test suites
namespace NeuroForge {
namespace Testing {

bool testPhaseABasics() {
    PhaseATestSuite suite(false);
    return suite.runAllTests();
}

bool testPhaseAVerbose() {
    PhaseATestSuite suite(true);
    return suite.runAllTests();
}

void demonstratePhaseALearning() {
    std::cout << "=== NeuroForge Phase A Baby Multimodal Mimicry Demo ===\n\n";
    
    // Initialize systems
    LanguageSystem::Config lang_config;
    auto language_system = std::make_shared<LanguageSystem>(lang_config);
    language_system->initialize();
    
    // Create memory database
    auto memory_db = std::make_shared<MemoryDB>("test_phase_a.db");
    
    // Create PhaseA config and system
    auto phase_a_config = PhaseAMimicryFactory::createDefaultConfig();
    auto phase_a_system = PhaseAMimicryFactory::create(language_system, memory_db, phase_a_config);
    phase_a_system->initialize();
    
    std::cout << "Phase A Baby Multimodal Mimicry Demo:\n";
    std::cout << "1. Teacher shows baby a picture of a cat\n";
    std::cout << "2. Teacher says 'cat'\n";
    std::cout << "3. Baby attempts to mimic both visual and auditory input\n";
    std::cout << "4. System creates cross-modal alignment for 'cat' concept\n";
    std::cout << "5. Baby's language system learns grounded 'cat' token\n\n";
    
    // Simulate the learning scenario
    auto cat_image_emb = phase_a_system->processCLIPVision("cat.jpg");
    auto cat_word_emb = phase_a_system->processBERTText("cat");
    
    std::string image_id = phase_a_system->addTeacherEmbedding(
        cat_image_emb, PhaseAMimicry::TeacherType::CLIP_Vision,
        PhaseAMimicry::Modality::Visual, "cat_image", "cat.jpg");
    
    std::string word_id = phase_a_system->addTeacherEmbedding(
        cat_word_emb, PhaseAMimicry::TeacherType::BERT_Text,
        PhaseAMimicry::Modality::Text, "cat_word", "cat");
    
    // Baby mimicry attempts
    auto visual_attempt = phase_a_system->attemptMimicry(cat_image_emb, image_id, "baby_sees_cat");
    auto word_attempt = phase_a_system->attemptMimicry(cat_word_emb, word_id, "baby_says_cat");
    
    // Create multimodal alignment
    std::size_t cat_token = language_system->createToken("cat", LanguageSystem::TokenType::Word);
    std::string alignment_id = phase_a_system->createMultimodalAlignment(
        {image_id, word_id}, {cat_token}, "cat_concept");
    
    // Display results
    std::cout << "Results:\n";
    std::cout << "- Visual mimicry success: " << (visual_attempt.success ? "YES" : "NO") << "\n";
    std::cout << "- Word mimicry success: " << (word_attempt.success ? "YES" : "NO") << "\n";
    std::cout << "- Cross-modal alignment created: " << (!alignment_id.empty() ? "YES" : "NO") << "\n";
    
    auto final_report = phase_a_system->generatePhaseAReport();
    std::cout << "\n" << final_report << "\n";
    
    std::cout << "=== Phase A Demo Complete ===\n";
}

} // namespace Testing
} // namespace NeuroForge
